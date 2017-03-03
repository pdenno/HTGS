
// NIST-developed software is provided by NIST as a public service. You may use, copy and distribute copies of the software in any medium, provided that you keep intact this entire notice. You may improve, modify and create derivative works of the software or any portion of the software, and you may copy and distribute such modifications or works. Modified works should carry a notice stating that you changed the software and should note the date and nature of any such change. Please explicitly acknowledge the National Institute of Standards and Technology as the source of the software.
// NIST-developed software is expressly provided "AS IS." NIST MAKES NO WARRANTY OF ANY KIND, EXPRESS, IMPLIED, IN FACT OR ARISING BY OPERATION OF LAW, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, NON-INFRINGEMENT AND DATA ACCURACY. NIST NEITHER REPRESENTS NOR WARRANTS THAT THE OPERATION OF THE SOFTWARE WILL BE UNINTERRUPTED OR ERROR-FREE, OR THAT ANY DEFECTS WILL BE CORRECTED. NIST DOES NOT WARRANT OR MAKE ANY REPRESENTATIONS REGARDING THE USE OF THE SOFTWARE OR THE RESULTS THEREOF, INCLUDING BUT NOT LIMITED TO THE CORRECTNESS, ACCURACY, RELIABILITY, OR USEFULNESS OF THE SOFTWARE.
// You are solely responsible for determining the appropriateness of using and distributing the software and you assume all risks associated with its use, including but not limited to the risks and costs of program errors, compliance with applicable laws, damage to or loss of data, programs or equipment, and the unavailability or interruption of operation. This software is not intended to be used in any situation where a failure could cause risk of injury or damage to property. The software developed by NIST employees is not subject to copyright protection within the United States.

/**
 * @file ExecutionPipeline.hpp
 * @author Timothy Blattner
 * @date Nov 30, 2015
 *
 * @brief ExecutionPipeline encapsulates a task graph and duplicates it, such that each
 * duplicate task graph executes concurrently.
 *
 */


#ifndef HTGS_EXECUTIONPIPELINE_H
#define HTGS_EXECUTIONPIPELINE_H

#include <cstring>

#include <htgs/core/rules/ExecutionPipelineBroadcastRule.hpp>
#include "ITask.hpp"
#include "Bookkeeper.hpp"
#include "TaskGraph.hpp"
#include "Runtime.hpp"
#include "IData.hpp"

namespace htgs {
template<class V, class W>
class IRule;

template<class V, class W>
class TaskGraph;

/**
 * @class ExecutionPipeline ExecutionPipeline.hpp <htgs/api/ExecutionPipeline.hpp>
 * @brief The ExecutionPipeline class is used to duplicate task graphs, such that each duplicate executes concurrently.
 * @details Each task within the task graph is an exact duplicate of the original's. The only
 * difference is the pipelineId passed to each task's initialize function. The pipelineId indicates which task graph
 * the task belongs to. This value can be used as a rank to distribute computation and determine if additional
 * functionality is needed to process data (such as copying data from one GPU to another).
 *
 * An ExecutionPipeline can be used to execute across multiple GPUs on a single system. Any ICudaTask will
 * be bound to a separate GPU based on the CUContext array passed to the ICudaTask. The size of the CUContext
 * array should match the number of execution pipelines specified. If data exists between two GPUs, then additional
 * copying may be required, see ICudaTask for details and example usage for copying data between GPUs.
 *
 * The execution pipeline can be used to distribute data among each task graph by adding a rule that
 * uses the pipelineId parameter in an IRule. See addInputRule(IRule <T, T> *rule)
 *
 *
 * Example usage:
 * @code
 * htgs::TaskGraph<MatrixData, MatrixData> *subGraph = new htgs::TaskGraph<MatrixData, MatrixData>();
 * ... build subGraph
 *
 * int numPipelines = 3;
 * htgs::ExecutionPipeline<MatrixData, MatrixData> *execPipeline = new htgs::ExecutionPipeline<MatrixData, MatrixData>(numPipelines, subGraph);
 * execPipeline->addInputRule(new DecompRule());
 * PostProcessTask *taskOutsideExecPipeline = new PostProcessTask();
 *
 * htgs::TaskGraph<MatrixData, VoidData> *mainGraph = new htgs::TaskGraph<MatrixData, VoidData>();
 *
 * // Declares that the execPipeline connects to the input of the mainGraph
 * mainGraph->addGraphInputConsumer(execPipeline);
 * mainGraph->addEdge(execPipeline, taskOutsideExecPipeline);
 *
 * // Indicate that main thread is producing data for main graph
 * mainGraph->incrementGraphInputProducer();
 *
 * htgs::Runtime *runTime = new htgs::Runtime(mainGraph);
 *
 * while(hasDataToAdd)
 * {
 *   // Add data to graph
 *   mainGraph->produceData(data);
 * }
 *
 * mainGraph->finishProducingData();
 *
 * // Executes the main graph (spawns sub graphs for execPipeline)
 * runTime->executeAndWaitForRunTime();
 *
 * @endcode
 *
 * @tparam T the input type for the ExecutionPipeline ITask, must derive from IData.
 * @tparam U the output type for the ExecutionPipeline ITask, must derive from IData.
 */
template<class T, class U>
class ExecutionPipeline: public ITask<T, U> {
  static_assert(std::is_base_of<IData, T>::value, "T must derive from IData");
  static_assert(std::is_base_of<IData, U>::value, "U must derive from IData");

 public:
  /**
   * Creates an execution pipeline, which encapsulates a graph and duplicates it numPipelines times
   * @param numPipelines the number of times to duplicate the graph
   * @param graph the graph that the execution pipeline manages
   */
  ExecutionPipeline(size_t numPipelines, TaskGraph<T, U> *graph) {
    this->numPipelinesExec = numPipelines;
    this->graph = graph;
    this->inputBk = new Bookkeeper<T>();
    this->runtimes = new std::list<Runtime *>();
    this->inputRules = new std::list<std::shared_ptr<IRule<T, T>> >();
    this->graphs = new std::list<TaskGraph<T, U> *>();
  }

  /**
   * Creates an execution pipeline, which encapsulates a graph and duplicates it numPipelines times
   * @param numPipelines the number of times to duplicate the graph
   * @param graph the graph that the execution pipeline manages
   * @param rules the list of decomposition rules that will be used for this pipeline
   */
  ExecutionPipeline(size_t numPipelines, TaskGraph<T, U> *graph, std::list<std::shared_ptr<IRule<T, T> >> *rules) {
    this->numPipelinesExec = numPipelines;
    this->graph = graph;
    this->inputBk = new Bookkeeper<T>();
    this->runtimes = new std::list<Runtime *>();
    this->inputRules = rules;
    this->graphs = new std::list<TaskGraph<T, U> *>();
  }

  /**
   * Destructor that frees all memory allocated by the execution pipeline
   */
  ~ExecutionPipeline() {
    for (Runtime *runtime : *runtimes) {
      delete runtime;
      runtime = nullptr;
    }
    delete runtimes;
    runtimes = nullptr;

    delete inputBk;
    inputBk = nullptr;

    delete graphs;
    graphs = nullptr;

    delete inputRules;
    inputRules = nullptr;
  }

  /**
   * Adds an input rule, which can be used for domain decomposition.
   * This rule should use the pipelineId parameter in the IRule::applyRule function to aid in
   * distributing data to the appropriate execution pipeline TaskGraph
   * @param rule the rule to handle distributing data between pipelines
   */
  void addInputRule(IRule<T, T> *rule) {
    this->inputRules->push_back(std::shared_ptr<IRule<T, T>>(rule));
  }

  /**
   * Initializes the execution pipeline and duplicates the task graph based on the number of pipelines.
   *
   * @note This function should only be called by the HTGS API
   */
  void initialize() {
    DEBUG("Initializing Execution pipeline with " << this->numPipelinesExec << " pipelines");

    // Add a default broadcast rule if the pipeline has no rules
    if (this->inputRules->size() == 0)
    {
      std::cerr << "Warning: Your execution pipeline does not have any decomposition rules... Adding the default broadcast rule" << std::endl;
      this->addInputRule(new ExecutionPipelineBroadcastRule<T>());
    }


    this->graph->updateIdAndNumPipelines(0, this->numPipelinesExec);

    DEBUG("Adding pipeline 0");
    for (std::shared_ptr<IRule<T, T>> rule : *this->inputRules) {

      RuleScheduler<T, T> *ruleScheduler = new RuleScheduler<T, T>(rule);
      ruleScheduler->setOutputConnector(graph->getInputConnector());
      ruleScheduler->initialize(0, this->numPipelinesExec);

      this->inputBk->addRuleScheduler(ruleScheduler);
    }

    std::shared_ptr<Connector<U>> outputConnector = std::static_pointer_cast<Connector<U>>(this->getOwnerTaskScheduler()->getOutputConnector());

    // Update outputs for task graph to go to the execution pipeline output
    graph->setOutputConnector(outputConnector);
    outputConnector->incrementInputTaskCount();

    this->graph->incrementGraphProducer();
    graphs->push_back(graph);

    for (size_t i = 1; i < numPipelinesExec; i++) {
      DEBUG("Adding pipeline " << i);
      TaskGraph<T, U> *graphCopy = this->graph->copy(i, this->numPipelinesExec, nullptr, outputConnector);

      DEBUG("Setting up input and output of pipeline " << i);

      for (std::shared_ptr<IRule<T, T>> rule : *this->inputRules) {

        RuleScheduler<T, T> *ruleScheduler = new RuleScheduler<T, T>(rule);
        ruleScheduler->setOutputConnector(graphCopy->getInputConnector());
        ruleScheduler->initialize(i, this->numPipelinesExec);

        this->inputBk->addRuleScheduler(ruleScheduler);
      }

      graphCopy->incrementGraphProducer();
      graphs->push_back(graphCopy);

    }

    for (TaskGraph<T, U> *g : *graphs) {
      Runtime *runtime = new Runtime(g);
      runtime->executeRuntime();
      this->runtimes->push_back(runtime);
    }

  }

  /**
   * Shuts down the execution pipeline.
   * @note This function should only be called by the HTGS API
   */
  void shutdown() {
    DEBUG("Shutting down " << this->getName());
    this->inputBk->shutdown();

    for (Runtime *r : *this->runtimes) {
      r->waitForRuntime();
    }
  }

  /**
   * Executes the execution pipeline task on data and forwards that data to the input rules.
   * The input rules should parse the data and determine the correct pipelineId to forward the data to.
   * @param data the data to be forwarded to the proper execution pipeline
   * @note This function should only be called by the HTGS API
   */
  void executeTask(std::shared_ptr<T> data) {
    if (data != nullptr) {
      this->inputBk->executeTask(data);
    }
  }

  /**
   * Gets the name for the execution pipeline
   * @return the name of the execution pipeline
   */
  std::string getName() {
    return "Execution Pipeline";
  }

  /**
   * Makes a copy of the execution pipeline.
   * Copies the graph and reuses the same input decomposition rules
   * @return the copied execution pipeline task
   * @note This function should only be called by the HTGS API
   */
  ITask<T, U> *copy() {
    return new ExecutionPipeline<T, U>(this->numPipelinesExec, this->graph->copy(0, 1), this->inputRules);
  }

  /**
   * Provides debugging output for the execution pipeline.
   * @note \#define DEBUG_FLAG to enable debugging
   */
  void debug() {
    DEBUG(this->getName() << " " << numPipelinesExec << " pipelines; details:");
    inputBk->debug();
  }

#ifdef PROFILE
  void gatherComputeTime(std::unordered_multimap<std::string, long long int> *mmap)
  {
    for (auto g : *graphs)
    {
      g->gatherComputeTime(mmap);
    }
  }


  void gatherWaitTime(std::unordered_multimap<std::string, long long int> *mmap)
  {
    for (auto g : *graphs)
    {
      g->gatherWaitTime(mmap);
    }
  }

  void gatherMaxQSize(std::unordered_multimap<std::string, int> *mmap)
  {
    for (auto g : *graphs)
    {
      g->gatherMaxQSize(mmap);
    }
  }

  std::string getDotProfile(int flags,
                            std::unordered_map<std::string, double> *mmap, double val,
                            std::string desc, std::unordered_map<std::string, std::string> *colorMap)
  {
    std::ostringstream oss;

    if (graphs->size() > 0)
    {
      for (auto g : *graphs)
      {
        oss << g->genProfileGraph(flags, mmap, desc, colorMap);
      }
    }
    else
    {
      oss << graph->genProfileGraph(flags, mmap, desc, colorMap);
    }

    return oss.str();
  }

#endif

  /**
   * Virtual function that adds additional dot attributes to this node.
   * @param idStr the id string for this task
   * @param inputConn the input connector for this task
   * @param outputConn the output connector for this task
   * @return the additional dot attributes for the dot graph representation
   */
  std::string genDot(int flags, std::string idStr, std::shared_ptr<AnyConnector> inputConn, std::shared_ptr<AnyConnector> outputConn) override {
    std::ostringstream oss;

    oss << inputConn->genDot(flags);

    // Get inputRule edge name
    std::string inputRuleNames;
    int count = 0;
    for (std::shared_ptr<IRule<T, T>> rule : *inputRules) {
      if (count == 0)
        inputRuleNames = inputRuleNames + rule->getName();
      else
        inputRuleNames = inputRuleNames + ", " + rule->getName();
      count++;
    }

    // Draw decomposition rule edge to each graph
    if (graphs->size() > 0)
    {
      for (auto g : *graphs)
      {
        oss << inputConn->getDotId() << " -> " << g->getInputConnector()->getDotId() << "[label=\"" << inputRuleNames
            << "\"];" << std::endl;
      }
    }
    else {
      oss << inputConn->getDotId() << " -> " << graph->getInputConnector()->getDotId() << "[label=\"" << inputRuleNames
          << "\"];" << std::endl;
    }

    // Setup subgraph to draw execPipeline graphs
    if (this->graphs->size() > 0)
    {
      int pipeline = 0;
      for (auto g : *graphs)
      {
        oss << "subgraph cluster_" << idStr << std::to_string(pipeline) <<" {" << std::endl;
        oss << "label=\"" << getName() << std::to_string(pipeline) << "\";" << std::endl;
        oss << "style=filled;" << std::endl;
        oss << "color=lightgrey;" << std::endl;
        oss << g->genDotGraphContent(flags);
        oss << "}" << std::endl;
        pipeline++;
      }
    }
    else {
      oss << "subgraph cluster_" << idStr << " {" << std::endl;
      oss << "label=\"" << getName() << " x" << this->numPipelinesExec << "\";" << std::endl;
      oss << "style=filled;" << std::endl;
      oss << "color=lightgrey;" << std::endl;

      graph->setOutputConnector(outputConn);
      oss << graph->genDotGraphContent(flags);
      oss << "}" << std::endl;
    }

    return oss.str();
  }


 private:
  size_t numPipelinesExec; //!< The number of pipelines that will spawn from the ExecutionPipeline
  Bookkeeper<T> *inputBk; //!< The input Bookkeeper for the ExecutionPipeline
  TaskGraph<T, U> *graph; //!< The graph that the ExecutionPipeline manages, duplicates, and executes
  std::list<std::shared_ptr<IRule<T, T>>> *inputRules; //!< The rules associated with the input Bookkeeper for decomposition
  std::list<Runtime *> *runtimes; //!< The list of Runtimes that will execute the TaskGraphs (one for each duplicate TaskGraph)
  std::list<TaskGraph<T, U> *> *graphs; //!< The list of duplicate TaskGraphs
};
}

#endif //HTGS_EXECUTIONPIPELINE_H
