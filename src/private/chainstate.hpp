// Copyright (C) 2021 The Xaya developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef XAYAX_CHAINSTATE_HPP
#define XAYAX_CHAINSTATE_HPP

#include "blockdata.hpp"
#include "private/database.hpp"

#include <cstdint>
#include <string>

namespace xayax
{

/**
 * Storage abstraction for the known state of the underlying blockchain.
 * This is mainly a database of all the block headers (with basic metadata)
 * that we are aware of, together with the structure they form as a tree
 * based on some genesis block (which may be a block later than the
 * actual blockchain genesis).
 *
 * The tree structure allows us to handle reorgs properly, and also to determine
 * all blocks that need to be detached for an arbitrary reorg of a GSP
 * (i.e. game_sendupdates) back to the main chain.
 *
 * The database contains full move data for recent blocks as well as blocks
 * not on the main chain, as we cannot extract the move data for those
 * blocks from the base blockchain API, and they are expected by GSPs for
 * reorgs / detaching of blocks.  The number of such blocks is limited, so
 * that storing the full move data for them is not a big issue.  For blocks
 * on the main chain that are already buried deeply (so they won't end up
 * on a branch in the end), we remove that data.
 *
 * Internally, each block in our database has a "branch number".  This is
 * zero for blocks on the main chain, and a larger integer for blocks on a
 * branch.  With a database index on the branch number and block height,
 * we can easily handle common reorg tasks (e.g. mark all blocks of the old
 * chain following some fork point as on a branch, detect the fork point
 * for a given block to the main chain) with simple database queries.
 *
 * As with the database, this class is not thread-safe and must be externally
 * synchronised as needed.
 */
class Chainstate : private Database
{

public:

  class UpdateBatch;

  /**
   * Constructs the instance, using the given file as underlying SQLite
   * database for state storage.  The file is created as a new database
   * if it does not yet exist.
   */
  explicit Chainstate (const std::string& file);

  /**
   * If no chain string is recorded yet in the local database,
   * sets it to the given value.  If one is set, verifies that it
   * matches the value; aborts if not (as this would mean we are
   * mixing up inconsistent chains).
   */
  void SetChain (const std::string& chain);

  /**
   * Returns the block height of the best chain.  If there is no block
   * set yet (not even a genesis block), returns -1.
   */
  int64_t GetTipHeight () const;

  /**
   * Returns the block hash corresponding to a given height in the current
   * main chain.  Returns true on success and false if the block height
   * is not known.
   */
  bool GetHashForHeight (uint64_t height, std::string& hash) const;

  /**
   * Returns the block height corresponding to a given hash if it is
   * known.
   */
  bool GetHeightForHash (const std::string& hash, uint64_t& height) const;

  /**
   * Initialises the chain state by importing the genesis block.  This
   * clears all other data (if any).
   */
  void Initialise (const BlockData& genesis);

  /**
   * Attaches a new block as best tip.  If the tip cannot be attached (because
   * its parent block is unknown), false is returned.  Otherwise the chainstate
   * is updated accordingly, true is returned, and the previous tip's hash
   * is set.
   */
  bool SetTip (const BlockData& blk, std::string& oldTip);

  /**
   * Determines the fork point and branch that connects a given block (by hash)
   * to the current main chain.  Returns false if the given block hash is
   * not known.  Otherwise, true is returned.
   *
   * In that case, the branch is filled in with the blocks that need to be
   * detached to go from the requested block to a block whose parent hash
   * is on the main chain; the first element in the branch will be the
   * requested block itself.  If the requested block actually is on the
   * main chain, then an empty branch is returned instead.
   */
  bool GetForkBranch (const std::string& hash,
                      std::vector<BlockData>& branch) const;

  /**
   * Prunes all data of blocks on the main chain at or below the given height.
   * This in essence asserts that those blocks will certainly not end up on a
   * reorg in the future.
   */
  void Prune (uint64_t untilHeight);

  /**
   * Runs a sanity check on the stored state, verifying some assumed conditions.
   * Aborts if anything is wrong.  This method can take a long time, and is
   * meant for testing, not production.
   */
  void SanityCheck () const;

};

/**
 * Helper class that performs a batched update of the database using RAII
 * semantics.  When created, it will place a savepoint in the database,
 * which can then either be committed explicitly on success, or will
 * be reverted otherwise in the destructor.
 */
class Chainstate::UpdateBatch
{

private:

  /** The associated chainstate instance.  */
  Chainstate& parent;

  /** Set to true if the update has been committed.  */
  bool committed = false;

public:

  explicit UpdateBatch (Chainstate& p);
  ~UpdateBatch ();

  /**
   * Marks this update as being succeeded, which will commit the
   * underlying database savepoint.
   */
  void Commit ();

};

} // namespace xayax

#endif // XAYAX_CHAINSTATE_HPP
