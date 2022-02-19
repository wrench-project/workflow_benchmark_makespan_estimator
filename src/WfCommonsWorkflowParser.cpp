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


#include <iostream>
#include <vector>
#include <fstream>
#include <pugixml.hpp>
#include <nlohmann/json.hpp>


/**
 * Documentation in .h file
 */
std::shared_ptr<wrench::Workflow> WfCommonsWorkflowParser::createWorkflowFromJSON(const std::string &filename,
                                                                                  double flops_per_unit_of_cpu_work,
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

    for (nlohmann::json::iterator it = workflowJobs.begin(); it != workflowJobs.end(); ++it) {
        if (it.key() == "tasks") {
            std::vector<nlohmann::json> jobs = it.value();

            for (auto &job : jobs) {
                std::cerr << ".";
                fflush(stderr);
                std::string name = job.at("name");
                std::string type = job.at("type");
                if (type != "compute") {
                    throw std::invalid_argument("Workflow::createWorkflowFromJson(): Job " + name + " has unknown type " + type);
                }

                // Finding out the CPU work argument
                auto arguments = job.at("command").at("arguments");
                std::string cpu_work_arg = arguments.at(2);
                std::vector<std::string> tokens;
                boost::split(tokens, cpu_work_arg, boost::is_any_of(" "));
                double cpu_work = std::stod(tokens.at(1));

                // Figuring out flops
                double flops = cpu_work * flops_per_unit_of_cpu_work;

                task = workflow->addTask(name, flops, 1, 1, 0.0);

                // task priority
                try {
                    task->setPriority(job.at("priority"));
                } catch (nlohmann::json::out_of_range &e) {
                    // do nothing
                }

                // task average CPU
                try {
                    task->setAverageCPU(job.at("avgCPU"));
                } catch (nlohmann::json::out_of_range &e) {
                    // do nothing
                }

                // task bytes read
                try {
                    task->setBytesRead(job.at("bytesRead"));
                } catch (nlohmann::json::out_of_range &e) {
                    // do nothing
                }

                // task bytes written
                try {
                    task->setBytesWritten(job.at("bytesWritten"));
                } catch (nlohmann::json::out_of_range &e) {
                    // do nothing
                }

                // task files
                std::vector<nlohmann::json> files = job.at("files");

                for (auto &f : files) {
                    double size = f.at("size");
                    std::string link = f.at("link");
                    std::string id = f.at("name");
                    std::shared_ptr<wrench::DataFile> workflow_file = nullptr;
                    // Check whether the file already exists
                    try {
                        workflow_file = workflow->getFileByID(id);
                    } catch (const std::invalid_argument &ia) {
                        // making a new file
                        workflow_file = workflow->addFile(id, size);
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
                    // Ignore transfer jobs declared as parents
                    if (ignored_transfer_jobs.find(parent) != ignored_transfer_jobs.end()) {
                        continue;
                    }
                    // Ignore auxiliary jobs declared as parents
                    if (ignored_auxiliary_jobs.find(parent) != ignored_auxiliary_jobs.end()) {
                        continue;
                    }
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
