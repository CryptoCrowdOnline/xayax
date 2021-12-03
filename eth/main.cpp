// Copyright (C) 2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "config.h"

#include "controller.hpp"

#include "ethchain.hpp"

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <cstdlib>
#include <iostream>

namespace
{

DEFINE_string (eth_rpc_url, "",
               "URL for the Ethereum JSON-RPC interface");
DEFINE_string (eth_ws_url, "",
               "URL for the Ethereum websocket endpoint");
DEFINE_string (accounts_contract, "",
               "Address of the Xaya accounts registry contract to use");

DEFINE_string (datadir, "",
               "base data directory for the Xaya X state");

DEFINE_int32 (port, 0,
              "the port where Xaya X should listen for RPC requests");
DEFINE_bool (listen_locally, true,
             "whether or not the RPC server should only bind on localhost");
DEFINE_string (zmq_address, "",
               "the address to bind the ZMQ publisher to");

DEFINE_int64 (genesis_height, -1,
              "height from which to start the local chain state");
DEFINE_int32 (max_reorg_depth, 1'000,
              "maximum supported depth of reorgs");

DEFINE_bool (sanity_checks, false,
             "whether or not to run slow sanity checks for testing");

} // anonymous namespace

int
main (int argc, char* argv[])
{
  google::InitGoogleLogging (argv[0]);

  gflags::SetUsageMessage ("Run Xaya X connector to Ethereum");
  gflags::SetVersionString (PACKAGE_VERSION);
  gflags::ParseCommandLineFlags (&argc, &argv, true);

  try
    {
      if (FLAGS_eth_rpc_url.empty ())
        throw std::runtime_error ("--eth_rpc_url must be set");
      if (FLAGS_accounts_contract.empty ())
        throw std::runtime_error ("--accounts_contract must be set");
      if (FLAGS_port == 0)
        throw std::runtime_error ("--port must be set");
      if (FLAGS_zmq_address.empty ())
        throw std::runtime_error ("--zmq_address must be set");
      if (FLAGS_datadir.empty ())
        throw std::runtime_error ("--datadir must be set");
      if (FLAGS_genesis_height == -1)
        throw std::runtime_error ("--genesis_height must be set");
      if (FLAGS_max_reorg_depth < 0)
        throw std::runtime_error ("--max_reorg_depth must not be negative");

      xayax::EthChain base(FLAGS_eth_rpc_url, FLAGS_eth_ws_url,
                           FLAGS_accounts_contract);
      base.Start ();

      xayax::Controller controller(base, FLAGS_datadir);

      /* We use the genesis height passed and determine the associated
         block hash.  The height must be already deeply confirmed, so it
         will not be reorged any more.  */
      const auto genesis = base.GetBlockRange (FLAGS_genesis_height, 1);
      if (genesis.size () != 1)
        throw std::runtime_error ("Genesis block is not yet on the base chain");
      LOG (INFO) << "Using block " << genesis[0].hash << " as genesis block";
      controller.SetGenesis (genesis[0].hash, FLAGS_genesis_height);
      controller.SetMaxReorgDepth (FLAGS_max_reorg_depth);

      controller.SetZmqEndpoint (FLAGS_zmq_address);
      controller.SetRpcBinding (FLAGS_port, FLAGS_listen_locally);
      if (FLAGS_sanity_checks)
        controller.EnableSanityChecks ();

      controller.Run ();
    }
  catch (const std::exception& exc)
    {
      std::cerr << "Error: " << exc.what () << std::endl;
      return EXIT_FAILURE;
    }
  catch (...)
    {
      std::cerr << "Exception caught" << std::endl;
      return EXIT_FAILURE;
    }

  return EXIT_SUCCESS;
}
