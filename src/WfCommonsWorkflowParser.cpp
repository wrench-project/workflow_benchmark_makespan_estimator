/**
 * Copyright (c) 2017-2021. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <WfCommonsWorkflowParser.h>
#include <wrench-dev.h>
#include <UnitParser.h>
#include <boost/algorithm/string.hpp>
#include <sys/time.h>


#include <iostream>
#include <vector>
#include <fstream>
#include <pugixml.hpp>
#include <nlohmann/json.hpp>


/**
 * Documentation in .h file
 */
std::shared_ptr<wrench::Workflow> WfCommonsWorkflowParser::createWorkflowFromJSON(const std::string &filename,
                                                                                  double task_execution_time,
                                                                                  bool redundant_dependencies) {

    std::ifstream file;
    nlohmann::json j;
    std::set<std::string> ignored_auxiliary_jobs;
    std::set<std::string> ignored_transfer_jobs;

    auto workflow = wrench::Workflow::createWorkflow();

    //handle the exceptions of opening the json file
    file.exceptions(std::ifstream::failbit | std::ifstream::badbit);
    try {
        file.open(filename);
        file >> j;
    } catch (const std::ifstream::failure &e) {
        throw std::invalid_argument("Workflow::createWorkflowFromJson(): Invalid Json file");
    }

    nlohmann::json workflowJobs;
    try {
        workflowJobs = j.at("workflow");
    } catch (std::out_of_range &e) {
        throw std::invalid_argument("Workflow::createWorkflowFromJson(): Could not find a workflow exit");
    }

    std::shared_ptr<wrench::WorkflowTask> task;

    struct timeval last_time;

    gettimeofday(&last_time, nullptr);
    for (nlohmann::json::iterator it = workflowJobs.begin(); it != workflowJobs.end(); ++it) {
        if (it.key() == "tasks") {
            std::vector<nlohmann::json> jobs = it.value();

            int count = 0;
            for (auto &job : jobs) {
                if (++count % 1000 == 0) {
                    struct timeval now;
                    gettimeofday(&now, nullptr);
                    double elapsed = ((now.tv_sec - last_time.tv_sec)*1000000.0 + now.tv_usec - last_time.tv_usec)/1000000.0;
                    fprintf(stderr, "%.2lf%%  (%.2lf seconds)\n", 100.0 * count / jobs.size(), elapsed);
                    gettimeofday(&last_time, nullptr);
                }

                std::string name = job.at("name");

                task = workflow->addTask(name, task_execution_time, 1, 1, 0.0);

                // task files
                std::vector<nlohmann::json> files = job.at("files");

                for (auto &f : files) {
                    double size = f.at("size");
                    std::string link = f.at("link");
                    std::string id = f.at("name");
                    std::shared_ptr<wrench::DataFile> workflow_file = nullptr;
                    // Add the file
                    try {
                        workflow_file = workflow->addFile(id, size);
                    } catch (const std::invalid_argument &ia) {
                        workflow_file = workflow->getFileByID(id);
                    }
                    if (link == "input") {
                        task->addInputFile(workflow_file);
                    } else if (link == "output") {
                        task->addOutputFile(workflow_file);
                    }

                }
            }

            // since tasks may not be ordered in the JSON file, we need to iterate over all tasks again
            for (auto &job : jobs) {
                try {
                    task = workflow->getTaskByID(job.at("name"));
                } catch (std::invalid_argument &e) {
                    // Ignored task
                    continue;
                }
                std::vector<nlohmann::json> parents = job.at("parents");
                // task dependencies
                for (auto &parent : parents) {
                    try {
                        auto parent_task = workflow->getTaskByID(parent);
                        workflow->addControlDependency(parent_task, task, redundant_dependencies);
                    } catch (std::invalid_argument &e) {
                        // do nothing
                    }
                }
            }
        }
    }
    file.close();

    return workflow;
}
