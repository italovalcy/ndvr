#ifndef _NDVR_SECURITY_HELPER_HPP_
#define _NDVR_SECURITY_HELPER_HPP_

#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-helpers.hpp>
#include <ndn-cxx/util/io.hpp>
#include <fstream>

namespace ndn {
namespace ndvr {

inline void
setupRootCert(const ndn::Name& subjectName, std::string filename) {
  ndn::KeyChain keyChain;
  try {
    /* cleanup: remove any possible existing certificate with the same name */
    keyChain.deleteIdentity(keyChain.getPib().getIdentity(subjectName));
  }
  catch (const std::exception& e) {
  }
  ndn::security::Identity subjectId = keyChain.createIdentity(subjectName);

  if (filename.empty())
    return;

  ndn::security::v2::Certificate cert = subjectId.getDefaultKey().getDefaultCertificate();
  std::ofstream os(filename);
  io::save(cert, filename);
}

inline void
setupRootCert(const ndn::Name& subjectName) {
  setupRootCert(subjectName, "");
}

inline ndn::security::SigningInfo
setupSigningInfo(const ndn::Name subjectName, const ndn::Name issuerName) {
  // 1. Create identity/key/certificate (unsigned certificate)
  ndn::KeyChain keyChain;
  try {
    /* cleanup: remove any possible existing certificate with the same name */
    keyChain.deleteIdentity(keyChain.getPib().getIdentity(subjectName));
  }
  catch (const std::exception& e) {
  }
  ndn::security::Identity subjectId = keyChain.createIdentity(subjectName);
  ndn::security::v2::Certificate certReq = subjectId.getDefaultKey().getDefaultCertificate();
  ndn::security::v2::Certificate cert;

  ndn::Name certificateName = certReq.getKeyName();
  certificateName.append("NA");
  certificateName.appendVersion();
  cert.setName(certificateName);
  cert.setContent(certReq.getContent());

  // 2. Sign the certificate using issuer key
  ndn::SignatureInfo signatureInfo;
  signatureInfo.setValidityPeriod(ndn::security::ValidityPeriod(ndn::time::system_clock::TimePoint(),
                                                                ndn::time::system_clock::now() + ndn::time::days(365)));
  keyChain.sign(cert, ndn::security::SigningInfo(keyChain.getPib().getIdentity(issuerName))
                                                   .setSignatureInfo(signatureInfo));

  // 3. Install the signed certificate
  auto id = keyChain.getPib().getIdentity(cert.getIdentity());
  auto keyCert = id.getKey(cert.getKeyName());
  keyChain.addCertificate(keyCert, cert);
  keyChain.setDefaultCertificate(keyCert, cert);

  return ndn::security::SigningInfo(ndn::security::SigningInfo::SIGNER_TYPE_ID, subjectName);
}

}  // namespace ndvr
}  // namespace ndn

#endif  // _NDVR_SECURITY_HELPER_HPP_
