#ifndef TERVEL_MEMORY_RC_DESCRIPTOR_POOL_H_
#define TERVEL_MEMORY_RC_DESCRIPTOR_POOL_H_

#include <atomic>
#include <utility>

#include <assert.h>
#include <stdint.h>

#include "tervel/memory/descriptor.h"
#include "tervel/memory/info.h"
#include "tervel/memory/rc/pool_element.h"
#include "tervel/memory/rc/pool_manager.h"
#include "tervel/memory/system.h"

namespace tervel {
namespace memory {

class Descriptor;

namespace rc {

class PoolManager;

/**
 * If true, then DescriptorPool shouldn't reuse old pool elements when being
 * asked, even if it's safe to fo so. Instead, elements should be stock-piled
 * and left untouched when they're returned to the pool. This allows the user to
 * view associations. Entirely for debug purposes.
 *
 * TODO(carlos): move this to member var
 */
constexpr bool NO_REUSE_MEM = false;

/**
 * Defines a pool of descriptor objects which is used to allocate descriptors
 * and to store them while they are not safe to delete.
 *
 * The pool is represented as two linked lists of descriptors: one for safe
 * elements and one for unsafe elements. The safe pool is known to only contain
 * items owned by the thread owning the DescriptorPool object, and the unsafe
 * pool contains items where other threads may still hold refrences to them.
 *
 * Further, the pool object has a parent who is shared amongst other threads.
 * When a pool is to be destroyed, it sends its remaining elements to the
 * parent, relinquishing ownership of said elements. A top-level pool has a null
 * parent. At the moment, it only makes sense to have a single top-level parent
 * representing the central pool for all threads, and several local pools for
 * each thread.
 */
class DescriptorPool {
 public:
  DescriptorPool(int pool_id, PoolManager *manager, int prefill=4)
      : pool_id_(pool_id)
      , manager_(manager) {
    this->reserve(prefill);
  }
  ~DescriptorPool() { this->send_to_manager(); }

  /**
   * Constructs and returns a descriptor. Arguments are forwarded to the
   * constructor of the given descriptor type. User should call free_descriptor
   * on the returned pointer when they are done with it to avoid memory leaks.
   */
  template<typename DescrType, typename... Args>
  Descriptor * get_descriptor(Args&&... args);

  /**
   * Once a user is done with a descriptor, they should free it with this
   * method.
   *
   * @param descr The descriptor to free.
   * @param dont_check Don't check if the descriptor is being watched before
   *   freeing it. Use this flag if you know that no other thread has had access
   *   to this descriptor.
   */
  void free_descriptor(Descriptor *descr, bool dont_check=false);

  /**
   * Allocates an extra `num_descriptors` elements to the pool.
   *
   * TODO(carlos): Currently, this just blindly allocates using new, but
   *   typical semantics are that this should ensure at least n elements in the
   *   list. This is more expensive as it requires a list traversal, and I don't
   *   think there's any real benefit to it. Further, should this method first
   *   try to reclaim elements from the managers before calling new?
   *
   *   (steven): I think we should track the number of elements with counters
   *     Then we can use them to keep the number of elements between an upper
   *     limit and a lower limit.  I think we should always check the Manager
   *     before asking the allocator
   *
   *   (carlos) But what benefit does it get us to use counters? We basically
   *     just get extra contention on the counters so that this one seldom-used
   *     method can have correct semantics.
   *
   */
  void reserve(int num_descriptors);


 private:
  // -------------------------
  // FOR DEALING WITH ELEMENTS
  // -------------------------

  /**
   * Gets a free element. The local pool is checked for one first, then the
   * manager if there are no local ones, and if all else fails, a new one is
   * allocated using new.
   *
   * @param allocate_new If true and there are no free elements to retrieve from
   *   the pool, a new one is allocated. Otherwise, nullptr is returned.
   */
  PoolElement * get_from_pool(bool allocate_new=true);


  // -------------------------
  // FOR DEALING WITH MANAGERS
  // -------------------------

  /**
   * Sends all elements managed by this pool to the parent pool. Same as:
   *   send_safe_to_manager();
   *   send_unsafe_to_manager();
   */
  void send_to_manager();

  /**
   * Sends the elements from the safe pool to the corresponding safe pool in
   * this pool's manager.
   */
  void send_safe_to_manager();

  /**
   * Sends the elements from the unsafe pool to the corresponding unsafe pool in
   * this pool's manager.
   */
  void send_unsafe_to_manager();

  /**
   * Gets the safe pool of the manager associated with this pool.
   */
  std::atomic<PoolElement *> & manager_safe_pool() {
    return manager_->pools_[pool_id_].safe_pool;
  }

  /**
   * Gets the unsafe pool of the manager associated with this pool.
   */
  std::atomic<PoolElement *> & manager_unsafe_pool() {
    return manager_->pools_[pool_id_].unsafe_pool;
  }


  // --------------------------------
  // DEALS WITH SAFE AND UNSAFE LISTS
  // --------------------------------

  /**
   * Releases the descriptor back to the safe pool. Caller relinquishes
   * ownership of descriptor. It's expected that the given descriptor was taken
   * from this pool to begin with.
   *
   * Adding a descriptor to the safe pool calls its 'advance_return_to_pool'
   * method and its destructor.
   */
  void add_to_safe(Descriptor* descr);

  /**
   * Releases the descriptor back to the unsafe pool. Caller relinquishes
   * ownership of descriptor. It's expected that the given descriptor was taken
   * from this pool to begin with.
   *
   * See notes on add_to_safe()
   */
  void add_to_unsafe(Descriptor* descr);

  /**
   * Try to move elements from the unsafe pool to the safe pool.
   */
  void try_clear_unsafe_pool(bool dont_check=false);


  // -------
  // MEMBERS
  // -------

  /**
   * Index into this pool's manager's pool array corresponding to this pool.
   */
  int pool_id_;

  /**
   * This pool's manager.
   */
  PoolManager *manager_;

  /**
   * A linked list of pool elements.  One can be assured that no thread will try
   * to access the descriptor of any element in this pool. They can't be freed
   * as some threads may still have access to the element itself and may try to
   * increment the refrence count.
   */
  PoolElement *safe_pool_ {nullptr};

  /**
   * A linked list of pool elements. Elements get released to this pool when
   * they're no longer needed, but some threads may still try to access the
   * descriptor in the element. After some time has passed, items generally move
   * from this pool to the safe_pool_
   */
  PoolElement *unsafe_pool_ {nullptr};

#ifdef DEBUG_POOL
  // TODO(carlos) what, exactly, do these keep track of?
  uint64_t safe_pool_count_ {0};
  uint64_t unsafe_pool_count_ {0};
#endif

};

// REVIEW(carlos) These methods are not part of the class, and should thus not
//   be labled static.

//TODO(carlos): These methods are static methods of the rc descriptor class
// They 
/**
 * This method is used to increment the reference count of the passed descriptor
 * object. If after increming the reference count the object is still at the
 * address (indicated by *a == value), it will call advance_watch.
 * If that returns true then it will return true.
 * Otherwise it decrements the reference count and returns false
 *
 * REVIEW(carlos) param directives are java-doc style. should be 3 directives
 *   for each parameter, each with a parameter name and a description.
 * REVIEW(carlos) parameter names of this function have gone out-of-sync with
 *   function in the cc file
 * @param the descriptor which needs rc protection, 
 * the address it was derferenced from, and the bitmarked value of it.
 */
static bool watch(Descriptor *descr, std::atomic<void *> *a, void *value);

/**
 * This method is used to decrement the reference count of the passed descriptor
 * object.
 * Then it will call advance_unwatch and decrement any related objects necessary
 *
 * REVIEW(carlos) see notes on watch
 * @param the descriptor which no longer needs rc protection.
 */
static void unwatch(Descriptor *descr);

/**
 * This method is used to determine if the passed descriptor is under rc
 * rc protection.
 * Internally calls advance_iswatch.
 *
 * REVIEW(carlos) see notes on watch
 * @param the descriptor to be checked for rc protection.
 */
static bool is_watched(Descriptor *descr);



/**
 * This method is used to remove a descriptor object that is conflict with
 * another threads operation.
 * It first checks the recursive depth before proceding. 
 * Next it protects against the object being reused else where by acquiring
 * either HP or RC watch on the object.
 * Then once it is safe it will call the objects complete function
 * This function must gurantee that after its return the object has been removed
 * It returns the value.
 * 
 * REVIEW(carlos) see notes on watch
 * @param a marked reference to the object and the address the object was
 * dereferenced from.
 */

static void * remove_descriptor(void *expected, std::atomic<void *> *address);


// IMPLEMENTATIONS
// ===============

template<typename DescrType, typename... Args>
Descriptor * DescriptorPool::get_descriptor(Args&&... args) {
  PoolElement *elem = this->get_from_pool();
  elem->init_descriptor<DescrType>(std::forward<Args>(args)...);
  return elem->descriptor();
}


}  // namespace rc
}  // namespace memory
}  // namespace tervel

#endif  // TERVEL_MEMORY_RC_DESCRIPTOR_POOL_H_
