
// NIST-developed software is provided by NIST as a public service. You may use, copy and distribute copies of the software in any medium, provided that you keep intact this entire notice. You may improve, modify and create derivative works of the software or any portion of the software, and you may copy and distribute such modifications or works. Modified works should carry a notice stating that you changed the software and should note the date and nature of any such change. Please explicitly acknowledge the National Institute of Standards and Technology as the source of the software.
// NIST-developed software is expressly provided "AS IS." NIST MAKES NO WARRANTY OF ANY KIND, EXPRESS, IMPLIED, IN FACT OR ARISING BY OPERATION OF LAW, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT AND DATA ACCURACY. NIST NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST DOES NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE SOFTWARE OR THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE CORRECTNESS, ACCURACY, RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
// You are solely responsible for determining the appropriateness of using and distributing the software and you assume all risks associated with its use, including but not limited to the risks and costs of program errors, compliance with applicable laws, damage to or loss of data, programs or equipment, and the unavailability or interruption of operation. This software is not intended to be used in any situation where a failure could cause risk of injury or damage to property. The software developed by NIST employees is not subject to copyright protection within the United States.

/**
 * @file VoidData.hpp
 * @author Timothy Blattner
 * @date Nov 16, 2015
 *
 * @brief VoidData is used for data that is empty/void.
 * @details
 * Often used when a ITask or TaskGraphConf does not have an input or output to another ITask
 */
#ifndef HTGS_VOIDDATA_HPP
#define HTGS_VOIDDATA_HPP

#include <limits.h>
#include <htgs/api/IData.hpp>

namespace htgs {

/**
 * @class VoidData VoidData.hpp <htgs/api/VoidData.hpp>
 *
 * @brief VoidData is used for data that is empty/void.
 *
 * Can be used with a ITask or TaskGraph that does not have an input or output to another ITask.
 *
 * Example
 * @code
 * // TaskGraph that has input type Data1, but no output type
 * TaskGraph<Data1, VoidData> task;
 * @endcode
 *
 */
class VoidData : public IData {
 public:
  /**
   * Constructs VoidData
   */
  VoidData() {}
};
}

#endif //HTGS_VOIDDATA_HPP
