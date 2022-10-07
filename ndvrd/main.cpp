/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ndvr-runner.hpp"

int main(int32_t argc, char** argv)
{
  std::string programName(argv[0]);

  std::string networkName;
  std::string routerName;
  int helloInterval = 0;
  std::string validationConfig;
  std::vector<std::string> namePrefixes;
  std::vector<std::string> faces;  // faces we will be listen (existing faceId or localUri to be created)
  std::vector<std::string> monitorFaces;  // list of face URIs we will monitor for nfd/faces/events

  int32_t opt;
  while ((opt = getopt(argc, argv, "dv:c:n:r:i:p:f:m:h")) != -1) {
    switch (opt) {
      case 'v':
        validationConfig = optarg;
        break;
      case 'n':
        networkName = optarg;
        break;
      case 'r':
        routerName = optarg;
        break;
      case 'i':
        helloInterval = strtol(optarg, NULL, 10);
        break;
      case 'p':
        namePrefixes.push_back(optarg);
        break;
      case 'f':
        faces.push_back(optarg);
        break;
      case 'm':
        monitorFaces.push_back(optarg);
        break;
      case 'h':
      default:
        ndn::ndvr::NdvrRunner::printUsage(programName);
        return EXIT_FAILURE;
    }
  }
  if (networkName.empty()) {
    std::cerr << "Missing mandatory argument: -n " << std::endl;
    ndn::ndvr::NdvrRunner::printUsage(programName);
    return EXIT_FAILURE;
  }
  if (routerName.empty()) {
    std::cerr << "Missing mandatory argument: -r" << std::endl;
    ndn::ndvr::NdvrRunner::printUsage(programName);
    return EXIT_FAILURE;
  }
  if (validationConfig.empty()) {
    std::cerr << "Missing mandatory argument: -v" << std::endl;
    ndn::ndvr::NdvrRunner::printUsage(programName);
    return EXIT_FAILURE;
  }
  if (faces.size()==0) {
    std::cerr << "You must specify at least one face: -f" << std::endl;
    ndn::ndvr::NdvrRunner::printUsage(programName);
    return EXIT_FAILURE;
  }

  ndn::ndvr::NdvrRunner runner(networkName, routerName, helloInterval, validationConfig, namePrefixes, faces, monitorFaces);

  try {
    runner.run();
  }
  catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
