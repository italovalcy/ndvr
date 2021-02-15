/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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

#ifndef NFD_DAEMON_FW_ASF_STRATEGY_HPP
#define NFD_DAEMON_FW_ASF_STRATEGY_HPP

#include "asf-measurements.hpp"
#include "asf-probing-module.hpp"
#include "fw/retx-suppression-exponential.hpp"
#include "fw/strategy.hpp"
#include <utility>



namespace nfd {
namespace fw {
namespace asf {

//class ProbingModule;

/** \brief Adaptive SRTT-based Forwarding Strategy
 *
 *  \see Vince Lehman, Ashlesh Gawande, Rodrigo Aldecoa, Dmitri Krioukov, Beichuan Zhang, Lixia Zhang, and Lan Wang,
 *       "An Experimental Investigation of Hyperbolic Routing with a Smart Forwarding Plane in NDN,"
 *       NDN Technical Report NDN-0042, 2016. http://named-data.net/techreports.html
 *
 *  \note This strategy is not EndpointId-aware.
 */
class MAsfStrategy : public Strategy
{
public:
  explicit
  MAsfStrategy(Forwarder& forwarder, const Name& name = getStrategyName());
  static const size_t TIMEOUT_THRESHOLD;
  static const Name&
  getStrategyName();

public: // triggers
  void
  afterReceiveInterest(const FaceEndpoint& ingress, const Interest& interest,
                       const shared_ptr<pit::Entry>& pitEntry) override;

  void
  beforeSatisfyInterest(const shared_ptr<pit::Entry>& pitEntry,
                        const FaceEndpoint& ingress, const Data& data) override;

  void
  afterReceiveNack(const FaceEndpoint& ingress, const lp::Nack& nack,
                   const shared_ptr<pit::Entry>& pitEntry) override;

private:
  void
  processParams(const PartialName& parsed);

  void
  forwardInterest(const Interest& interest, Face& outFace, const fib::Entry& fibEntry,
                  const shared_ptr<pit::Entry>& pitEntry, bool wantNewNonce = false);
  void
  sendProbe(const Interest& interest, const FaceEndpoint& ingress, const Face& faceToUse,
            const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry);

  Face*
  getBestFaceForForwarding(const Interest& interest, const Face& inFace,
                           const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry,
                           bool isNewInterest = true);
  void
  updateFaceChangeStats(Face* topFace, const Interest& interest);

  void
  onTimeoutOrNack(const Name& interestName, FaceId faceId, bool isNack);

  void
  sendNoRouteNack(const FaceEndpoint& ingress, const shared_ptr<pit::Entry>& pitEntry);

  void
  exploreAlternateFaces(Face* forwardedFace, const FaceEndpoint& ingress, const Interest& interest,
                        const fib::Entry& fibEntry, const shared_ptr<pit::Entry>& pitEntry);
private:
  // struct BestPrefixFace
  // {
  //   Face * currentFace;
  //   Face * contendingFace;
  //   uint64_t rounds = 0;
  // };
  AsfMeasurements m_measurements;
  ProbingModule m_probing;
  RetxSuppressionExponential m_retxSuppression;
  size_t m_nMaxSilentTimeouts = 0;

};

class MAsfStrategyInfo : public StrategyInfo
{
public:
  MAsfStrategyInfo();

  static constexpr int
  getTypeId()
  {
    return 1031;
  }

  void
  setIsProbe(bool isProbe){
    m_isProbe = isProbe;
  }
  bool
  getIsProbe(){
    return m_isProbe;
  }

  void
  setIsMulticasted(bool isMulticasted){
    m_isMulticasted = isMulticasted;
  }
  bool
  getIsMulticasted(){
    return m_isMulticasted;
  }

  void
  incrementRetries(){
    retryCount += 1;
  }

  size_t
  getRetries(){
    return retryCount;
  }

private:
  bool m_isProbe;
  bool m_isMulticasted;
  size_t retryCount;
};

} // namespace asf

using asf::MAsfStrategy;

} // namespace fw
} // namespace nfd

#endif // NFD_DAEMON_FW_ASF_STRATEGY_HPP
