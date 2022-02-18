/**
 * Copyright (c) 2017-2019. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <wrench-dev.h>
#include <string>
#include <memory>


    class Workflow;

    /**
     * @brief A class that implement methods to read workflow files 
     *        provided by the WfCommons project
     */
    class WfCommonsWorkflowParser {

    public:

        /**
         * @brief Create an abstract workflow based on a JSON file
         *
         * @param filename: the path to the JSON file
         * @param flops_per_unit_of_work: How many flops correspond on 1 unit of CPU work passed to the workflow task benchmark
         * @param redundant_dependencies: Workflows provided by WfCommons
         *                             sometimes include control/data dependencies between tasks that are already induced by
         *                             other control/data dependencies (i.e., they correspond to transitive
         *                             closures or existing edges in the workflow graphs). Passing redundant_dependencies=true
         *                             force these "redundant" dependencies to be added as edges in the workflow. Passing
         *                             redundant_dependencies=false will ignore these "redundant" dependencies. Most users
         *                             would likely pass "false".
         * @return a workflow
         * @throw std::invalid_argument
         *
         */
        static std::shared_ptr<wrench::Workflow> createWorkflowFromJSON(const std::string &filename,
                                                                        double flops_per_unit_of_cpu_work,
                                                                        bool redundant_dependencies);

    };



