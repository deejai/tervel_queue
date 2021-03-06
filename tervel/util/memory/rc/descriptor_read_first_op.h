/*
The MIT License (MIT)

Copyright (c) 2015 University of Central Florida's Computer Software Engineering
Scalable & Secure Systems (CSE - S3) Lab

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
#ifndef DESCRIPTOR_READ_FIRST_OP_H_
#define DESCRIPTOR_READ_FIRST_OP_H_

#include <tervel/util/info.h>
#include <tervel/util/progress_assurance.h>



namespace tervel {
namespace util {

class Descriptor;

namespace memory {
namespace rc {

/**
 * Class used for placement in the Op Table to complete an operation that failed
 *    to complete in a bounded number of steps
 */
class ReadFirstOp : public util::OpRecord {
 public:
  explicit ReadFirstOp(std::atomic<void *> *address) {
    address_ = address;
    value_.store(nullptr);
  }

  ~ReadFirstOp() {}

  /**
   * This function overrides the virtual function in the OpRecord class
   * It is called by the progress assurance scheme.
   */
  void help_complete();

  void *load();

 private:
  std::atomic<void *> *address_;
  std::atomic<void *> value_;
};  // ReadFirstOp class


}  // namespace rc
}  // namespace memory
}  // namespace util
}  // namespace tervel

#endif  // DESCRIPTOR_READ_FIRST_OP_H_
