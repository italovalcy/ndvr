///* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2020,  Regents of the University of California,
 *                           Arizona Board of Regents,
 *                           Colorado State University,
 *                           University Pierre & Marie Curie, Sorbonne University,
 *                           Washington University in St. Louis,
 *                           Beijing Institute of Technology,
 *                           The University of Memphis.
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "asf-strategy.hpp"
#include "algorithm.hpp"
#include "common/global.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

namespace nfd {
namespace fw {
namespace asf {

NFD_LOG_INIT(MAsfStrategy);
NFD_REGISTER_STRATEGY(MAsfStrategy);

const time::microseconds RETX_SUPPRESSION_INITIAL(10);
const time::microseconds RETX_SUPPRESSION_MAX(250);

MAsfStrategy::MAsfStrategy(Forwarder& forwarder, const Name& name)
  : Strategy(forwarder)
  , m_measurements(getMeasurements())
  , m_probing(m_measurements)
  , m_retxSuppression(RETX_SUPPRESSION_INITIAL,
                      RetxSuppressionExponential::DEFAULT_MULTIPLIER,
                      RETX_SUPPRESSION_MAX)
{
  ParsedInstanceName parsed = parseInstanceName(name);
  if (!parsed.parameters.empty()) {
    processParams(parsed.parameters);
  }

  if (parsed.version && *parsed.version != getStrategyName()[-1].toVersion()) {
    NDN_THROW(std::invalid_argument(
      "MAsfStrategy does not support version " + to_string(*parsed.version)));
  }
  this->setInstanceName(makeInstanceName(name, getStrategyName()));

  NFD_LOG_DEBUG("probing-interval=" << m_probing.getProbingInterval()
                << " n-silent-timeouts=" << m_nMaxSilentTimeouts);
}

const Name&
MAsfStrategy::getStrategyName()
{
  static Name strategyName("/localhost/nfd/strategy/m-asf/%FD%01");
  return strategyName;
}

static uint64_t
getParamValue(const std::string& param, const std::string& value)
{
  try {
    if (!value.empty() && value[0] == '-')
      NDN_THROW(boost::bad_lexical_cast());

    return boost::lexical_cast<uint64_t>(value);
  }
  catch (const boost::bad_lexical_cast&) {
    NDN_THROW(std::invalid_argument("Value of " + param + " must be a non-negative integer"));
  }
}

void
MAsfStrategy::processParams(const PartialName& parsed)
{
  for (const auto& component : parsed) {
    std::string parsedStr(reinterpret_cast<const char*>(component.value()), component.value_size());
    auto n = parsedStr.find("~");
    if (n == std::string::npos) {
      NDN_THROW(std::invalid_argument("Format is <parameter>~<value>"));
    }

    auto f = parsedStr.substr(0, n);
    auto s = parsedStr.substr(n + 1);
    if (f == "probing-interval") {
      m_probing.setProbingInterval(getParamValue(f, s));
    }
    else if (f == "n-silent-timeouts") {
      m_nMaxSilentTimeouts = getParamValue(f, s);
    }
    else {
      NDN_THROW(std::invalid_argument("Parameter should be probing-interval or n-silent-timeouts"));
    }
  }
}

void
MAsfStrategy::exploreAlternateFaces(Face* primaryFace, const FaceEndpoint& ingress, const Interest& interest,
                                   const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry)
{
  forwardInterest(interest, *primaryFace, fibEntry, pitEntry);

  //Explore an alternate face.
  Face* altFace = primaryFace;
  time::steady_clock::TimePoint oldestTime = time::steady_clock::now();
  for (const fib::NextHop& hop : fibEntry.getNextHops())
  {
    Face& hopFace = hop.getFace();
    // Don't send probe Interest back to the incoming face or use the same face
    // as the forwarded Interest or use a face that violates scope
    if (primaryFace->getId() == hopFace.getId() ||
        hopFace.getId() == ingress.face.getId() ||
        wouldViolateScope(ingress.face, interest, hopFace))
    {
      continue;
    }

    NamespaceInfo* namespaceInfo = m_measurements.getNamespaceInfo(pitEntry->getName());
    FaceInfo* faceInfo = namespaceInfo->getFaceInfo(hopFace.getId());
    if (faceInfo != nullptr)
    {
      NFD_LOG_TRACE("Face info exists for " << hopFace.getId());
      if(faceInfo->getLastRtt() == FaceInfo::RTT_NO_MEASUREMENT)
      {
        //Try all unprobed faces.
        NFD_LOG_TRACE("Forwarding to unprobed face " << hopFace.getId());
        forwardInterest(interest, hopFace, fibEntry, pitEntry);
      }
      else
      {
        //Choose the oldest timed out or nacked face.
        time::steady_clock::TimePoint lastForwarded = faceInfo->getLastTimeForwarded();
        if(lastForwarded < oldestTime)
        {
          oldestTime = lastForwarded;
          altFace = &hopFace;
        }
      }
    }
    else
    {
      //Treat as an unprobed face
      NFD_LOG_TRACE("Forwarding to face with no info " << hopFace.getId());
      forwardInterest(interest, hopFace, fibEntry, pitEntry, true);
    }
  }

  if(altFace->getId() != primaryFace->getId())
  {
    NFD_LOG_TRACE("Forwarding to oldest timed out or nacked face " << altFace->getId());
    forwardInterest(interest, *altFace, fibEntry, pitEntry, true);
  }
}

void
MAsfStrategy::afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                                  const shared_ptr<pit::Entry>& pitEntry)
{
  NamespaceInfo* namespaceInfo = m_measurements.getNamespaceInfo(pitEntry->getName());

  // Should the Interest be suppressed?
  auto suppressResult = m_retxSuppression.decidePerPitEntry(*pitEntry);
  if (suppressResult == RetxSuppressionResult::SUPPRESS) {
    NFD_LOG_DEBUG(interest << " retx-interest from=" << ingress << " suppressed");
    return;
  }

  const fib::Entry& fibEntry = this->lookupFib(*pitEntry);
  const fib::NextHopList& nexthops = fibEntry.getNextHops();

  if (nexthops.size() == 0) {
    NFD_LOG_DEBUG(interest << " interest from=" << ingress << " no-nexthop");
    if (suppressResult == RetxSuppressionResult::NEW) {
      sendNoRouteNack(ingress, pitEntry);
    }
    return;
  }

  //Get the best face and check to see if it's a good face.
  Face* faceToUse = getBestFaceForForwarding(interest, ingress.face, fibEntry, pitEntry);
  if (faceToUse == nullptr) {
    NFD_LOG_DEBUG(interest << " interest from=" << ingress << " no-nexthop");
    if (suppressResult == RetxSuppressionResult::NEW) {
      sendNoRouteNack(ingress, pitEntry);
    }
    return;
  }

  FaceInfo* faceInfo = m_measurements.getFaceInfo(fibEntry, interest, faceToUse->getId());
  
	if (faceInfo != nullptr && faceInfo->getLastRtt() != FaceInfo::RTT_NO_MEASUREMENT) {
    NFD_LOG_DEBUG("Need to update face change status. Topface: " << faceToUse->getId() );
    updateFaceChangeStats(faceToUse, interest);
  }

  //Check if the retrieved face is working and measured, explore if not
  if (faceInfo != nullptr && faceInfo->getLastRtt() != FaceInfo::RTT_TIMEOUT
      && faceInfo->getLastRtt() != FaceInfo::RTT_NO_MEASUREMENT
      && !faceInfo->isNacked()) {
    NamespaceInfo::BestPrefixFace* faceContest = namespaceInfo->getFaceContest();

    NFD_LOG_DEBUG("Send to primary face: " << faceContest->primaryFace->getId());
    forwardInterest(interest, *(faceContest->primaryFace), fibEntry, pitEntry);

    if (faceContest->contest) {
      NFD_LOG_DEBUG("Face contest is on. Also send to current top face: " << faceToUse->getId());
      forwardInterest(interest, *faceToUse, fibEntry, pitEntry, true);
    }
		// If necessary, send probe- checks if due in sendProbe
    sendProbe(interest, ingress, *faceToUse, fibEntry, pitEntry);
  }
  else {
    NFD_LOG_DEBUG("Top face does not have measurement or timed-out or nacked. Need to explore alternate faces.");
    exploreAlternateFaces(faceToUse, ingress, interest, fibEntry, pitEntry);
  }
}

void
MAsfStrategy::beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                                   const FaceEndpoint& ingress, const Data& data)
{
  NamespaceInfo* namespaceInfo = m_measurements.getNamespaceInfo(pitEntry->getName());
  if (namespaceInfo == nullptr) {
    NFD_LOG_DEBUG(pitEntry->getName() << " data from=" << ingress << " no-measurements");
    return;
  }

  // Record the RTT between the Interest out to Data in
  FaceInfo* faceInfo = namespaceInfo->getFaceInfo(ingress.face.getId());
  if (faceInfo == nullptr) {
    NFD_LOG_DEBUG(pitEntry->getName() << " data from=" << ingress << " no-face-info");
    return;
  }

  auto outRecord = pitEntry->getOutRecord(ingress.face);
  if (outRecord == pitEntry->out_end()) {
    NFD_LOG_DEBUG(pitEntry->getName() << " data from=" << ingress << " no-out-record");
  }
  else {
    faceInfo->recordRtt(time::steady_clock::now() - outRecord->getLastRenewed());
    NFD_LOG_DEBUG(pitEntry->getName() << " data from=" << ingress
                  << " rtt=" << faceInfo->getLastRtt() << " srtt=" << faceInfo->getSrtt());
  }

  // Extend lifetime for measurements associated with Face
  namespaceInfo->extendFaceInfoLifetime(*faceInfo, ingress.face.getId());
  // Extend PIT entry timer to allow slower probes to arrive
  this->setExpiryTimer(pitEntry, 100_ms);
  faceInfo->cancelTimeout(data.getName());
}

void
MAsfStrategy::afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                              const shared_ptr<pit::Entry>& pitEntry)
{
  NFD_LOG_DEBUG(nack.getInterest() << " nack from=" << ingress << " reason=" << nack.getReason());
  onTimeoutOrNack(pitEntry->getName(), ingress.face.getId(), true);
}

void
MAsfStrategy::forwardInterest(const Interest& interest, Face& outFace, const fib::Entry& fibEntry,
                             const shared_ptr<pit::Entry>& pitEntry, bool wantNewNonce)
{
  auto egress = FaceEndpoint(outFace, 0);
  if (wantNewNonce) {
    // Send probe: interest with new Nonce
    Interest probeInterest(interest);
    probeInterest.refreshNonce();
    NFD_LOG_TRACE("Sending probe for " << probeInterest << " to=" << egress);
    this->sendInterest(pitEntry, egress, probeInterest);
  }
  else {
    this->sendInterest(pitEntry, egress, interest);
  }

  FaceInfo& faceInfo = m_measurements.getOrCreateFaceInfo(fibEntry, interest, egress.face.getId());
  faceInfo.markLastTimeForwarded();

  // Refresh measurements since Face is being used for forwarding
  NamespaceInfo& namespaceInfo = m_measurements.getOrCreateNamespaceInfo(fibEntry, interest);
  namespaceInfo.extendFaceInfoLifetime(faceInfo, egress.face.getId());

  if (!faceInfo.isTimeoutScheduled()) {
    auto timeout = faceInfo.scheduleTimeout(interest.getName(),
      [this, name = interest.getName(), faceId = egress.face.getId()] {
        onTimeoutOrNack(name, faceId, false);
      });
    NFD_LOG_TRACE("Scheduled timeout for " << fibEntry.getPrefix() << " to=" << egress
                  << " in " << time::duration_cast<time::microseconds>(timeout) << " ms");
  }
}

void
MAsfStrategy::sendProbe(const Interest& interest, const FaceEndpoint& ingress, const Face& faceToUse,
                       const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry)
{
  NamespaceInfo::BestPrefixFace* faceContest = m_measurements.getNamespaceInfo(interest.getName())->getFaceContest();
  if (!m_probing.isProbingNeeded(fibEntry, interest)) {
    NFD_LOG_DEBUG("Probing is not needed.");
		return;
	}
  std::vector<Face*> facesToProbe = m_probing.getFacesToProbe(ingress.face, interest, fibEntry, *faceContest);
  if (facesToProbe.empty())
    return;
  for (Face* faceToProbe : facesToProbe) {
		NFD_LOG_DEBUG("Probing Face " << faceToProbe->getId());
    forwardInterest(interest, *faceToProbe, fibEntry, pitEntry, true);
  }
  m_probing.afterForwardingProbe(fibEntry, interest);
}

void
MAsfStrategy::updateFaceChangeStats(Face* topFace, const Interest& interest)
{
  NamespaceInfo::BestPrefixFace* faceContest = m_measurements.getNamespaceInfo(interest.getName())->getFaceContest();
  if (faceContest == nullptr) {
    return;
  }
  if (faceContest->primaryFace == nullptr) {
    NFD_LOG_DEBUG("Primary face is nullptr. Set it to current top face " << topFace->getId());
    faceContest->primaryFace = topFace;
    //faceContest->changeTime = boost::posix_time::microsec_clock::local_time();

    return;
  }

  //Is initialized?
  /*if (faceContest == nullptr) {
    NamespaceInfo::BestPrefixFace temp;
    temp.primaryFace = topFace;
    temp.bestFace = nullptr;
    temp.changeTime = boost::posix_time::microsec_clock::local_time();
    faceContest = &temp;
    m_measurements.getNamespaceInfo(interest.getName())->setFaceContest(faceContest);
    return;
  }*/

  //If top ranked face not different from current primary face
  if (topFace == faceContest->primaryFace) {
    NFD_LOG_DEBUG("Top ranked face (" << topFace->getId() << ") is same as primary face " << faceContest->primaryFace->getId());
    faceContest->contest = false;
    return;
  }
  ndn::time::steady_clock::duration threshold = boost::chrono::milliseconds(2000);
  if (faceContest->contest) {
    ndn::time::system_clock::TimePoint now = ndn::time::system_clock::now();
    NFD_LOG_DEBUG("Face contest is on. Check if the current top face (" << topFace->getId() << ") has been on top for the threshold time duration.");
    // TODO: Code style!
    boost::chrono::milliseconds elapsedTime = boost::chrono::duration_cast<boost::chrono::milliseconds>
    (now-faceContest->changeTime);
    if (elapsedTime >= threshold) {
      NFD_LOG_DEBUG("Changing the primary face from " << faceContest->primaryFace->getId() << " to " << topFace->getId());
      faceContest->primaryFace = topFace;
      faceContest->contest = false;
      faceContest->changeTime = ndn::time::system_clock::now();
    }
    else {
      NFD_LOG_DEBUG("Current top face has not been in the top for threshold duration. So don't change primary face.");
    }
  }
  else {
    NFD_LOG_DEBUG("Top ranked face (" << topFace->getId() << ") is different than primary face (" << faceContest->primaryFace->getId() << ") . Turn on face contest and store the change time.");
    faceContest->bestFace = topFace;
    faceContest->contest = true;
    faceContest->changeTime = ndn::time::system_clock::now();
  }
}

Face*
MAsfStrategy::getBestFaceForForwarding(const Interest& interest, const Face& inFace,
                                      const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry,
                                      bool isInterestNew)
{
  std::set<FaceStats, FaceStatsCompare> rankedFaces;

  auto now = time::steady_clock::now();
  for (const auto& nh : fibEntry.getNextHops()) {
    if (!isNextHopEligible(inFace, interest, nh, pitEntry, !isInterestNew, now)) {
      continue;
    }

    FaceInfo* info = m_measurements.getFaceInfo(fibEntry, interest, nh.getFace().getId());
    if (info == nullptr) {
      rankedFaces.insert({&nh.getFace(), FaceInfo::RTT_NO_MEASUREMENT,
                          FaceInfo::RTT_NO_MEASUREMENT, nh.getCost()});
    }
    else {
      rankedFaces.insert({&nh.getFace(), info->getLastRtt(), info->getSrtt(), nh.getCost()});
    }
  }

  NFD_LOG_DEBUG("Current ranking of the faces for forwarding: ");
  auto it1 = rankedFaces.begin();
  for(; it1 != rankedFaces.end(); it1++) {
    NFD_LOG_DEBUG("	Faceid: " << it1->face->getId() << ", rtt: " << it1->rtt << ", srtt: " << it1->srtt);
  }

  auto it = rankedFaces.begin();
  return it != rankedFaces.end() ? it->face : nullptr;
}

void
MAsfStrategy::onTimeoutOrNack(const Name& interestName, FaceId faceId, bool isNack)
{
  NamespaceInfo* namespaceInfo = m_measurements.getNamespaceInfo(interestName);
  if (namespaceInfo == nullptr) {
    NFD_LOG_TRACE(interestName << " FibEntry has been removed since timeout scheduling");
    return;
  }

  FaceInfo* fiPtr = namespaceInfo->getFaceInfo(faceId);
  if (fiPtr == nullptr) {
    NFD_LOG_TRACE(interestName << " FaceInfo id=" << faceId << " has been removed since timeout scheduling");
    return;
  }

  auto& faceInfo = *fiPtr;
  size_t nTimeouts = faceInfo.getNSilentTimeouts() + 1;
  faceInfo.setNSilentTimeouts(nTimeouts);

  if (nTimeouts <= m_nMaxSilentTimeouts && !isNack) {
    NFD_LOG_TRACE(interestName << " face=" << faceId << " timeout-count=" << nTimeouts << " ignoring");
    // Extend lifetime for measurements associated with Face
    namespaceInfo->extendFaceInfoLifetime(faceInfo, faceId);
    faceInfo.cancelTimeout(interestName);
  }
  else {
    if (isNack) {
      NFD_LOG_TRACE(interestName << " face=" << faceId << " nacked");
      faceInfo.recordNack(interestName);
    }
    else {
      NFD_LOG_TRACE(interestName << " face=" << faceId << " timeout-count=" << nTimeouts);
      faceInfo.recordTimeout(interestName);
    }
  }
}

void
MAsfStrategy::sendNoRouteNack(const FaceEndpoint& ingress, const shared_ptr<pit::Entry>& pitEntry)
{
  lp::NackHeader nackHeader;
  nackHeader.setReason(lp::NackReason::NO_ROUTE);
  this->sendNack(pitEntry, ingress, nackHeader);
  this->rejectPendingInterest(pitEntry);
}

} // namespace asf
} // namespace fw
} // namespace nfd
