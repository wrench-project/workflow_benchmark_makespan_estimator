
/**
 * Copyright (c) 2017-2022. The WRENCH Team.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <iostream>
#include <wrench-dev.h>
#include <boost/program_options.hpp>
#include <random>
#include <UnitParser.h>
#include <wrench/tools/wfcommons/WfCommonsWorkflowParser.h>
#include <WfCommonsWorkflowParser.h>
#include <boost/algorithm/string.hpp>

#define GFLOP (1000.0 * 1000.0 * 1000.0)
#define TFLOP (1000.0 * 1000.0 * 1000.0 * 1000.0)
#define MBYTE (1000.0 * 1000.0)
#define GBYTE (1000.0 * 1000.0 * 1000.0)

namespace po = boost::program_options;

struct platform_spec {
    unsigned num_cores_per_node;
    double cpu_task_execution_time;
    double mem_task_execution_time;
    double io_read_speed_per_node;
    double io_write_speed_per_node;
};

std::map<std::string, struct platform_spec> platform_specs = {
    { "Summit",
        {
                40,
                20.624, // time python3 wfbench.py --percent-cpu 0.9 --cpu-work 500 abc
                2 * 60.0 + 47.927, // time python3 wfbench.py --percent-cpu 0.1 --cpu-work 500 abc
                466 * MBYTE, // time dd of=/dev/zero if=test-file iflag=direct bs=128k count=4k
                59.9 * MBYTE // time dd if=/dev/zero of=test-file oflag=direct bs=128k count=4k
        }
    },
    { "Piz Daint",
        {
                36,
                7.132, // time python3 wfbench.py --percent-cpu 0.9 --cpu-work 500 abc
                53.690, // time python3 wfbench.py --percent-cpu 0.1 --cpu-work 500 abc
                45.3 * MBYTE, // time dd of=/dev/zero if=test-file iflag=direct bs=128k count=4k
                13.3 * MBYTE // time dd if=/dev/zero of=test-file oflag=direct bs=128k count=4k
        }
    }
};

double compute_total_work(std::shared_ptr<wrench::Workflow> workflow) {
    double total_flops = 0.0;
    for (auto const &t : workflow->getTasks()) {
        total_flops += t->getFlops();
    }
    return total_flops;
}

std::pair<double, double> compute_total_data(const std::shared_ptr<wrench::Workflow>& workflow) {
    double total_read_data = 0.0;
    double total_written_data = 0.0;
    for (auto const &t : workflow->getTasks()) {
        for (auto const &f: t->getInputFiles()) {
            total_read_data += f->getSize();
        }
        for (auto const &f: t->getOutputFiles()) {
            total_written_data += f->getSize();
        }
    }
    return std::make_pair(total_read_data, total_written_data);
}

double estimate_makespan_naive_no_overlap(const std::shared_ptr<wrench::Workflow>& workflow,
                                          unsigned long num_nodes,
                                          unsigned long num_cores_per_node,
                                          double io_read_speed_per_node,
                                          double io_write_speed_per_node) {

    auto total_data = compute_total_data(workflow);
    double total_read_data = std::get<0>(total_data);
    double total_written_data = std::get<1>(total_data);

    double io_read_time = total_read_data / (io_read_speed_per_node * (double)num_nodes);
    std::cerr << "\nIO READ TIME = " << io_read_time << "\n";
    double compute_time = compute_total_work(workflow) / ((double)num_nodes * (double)num_cores_per_node);
    std::cerr << "COMPUTE TIME = " << compute_time << "\n";
    double io_write_time = total_written_data / (io_write_speed_per_node * (double)num_nodes);
    std::cerr << "IO WRITE TIME = " << io_write_time << "\n";

    return io_read_time + compute_time + io_write_time;
}

double estimate_makespan_naive_overlap(const std::shared_ptr<wrench::Workflow>& workflow,
                                       unsigned long num_nodes,
                                       unsigned long num_cores_per_node,
                                       double io_read_speed_per_node,
                                       double io_write_speed_per_node) {

    auto total_data = compute_total_data(workflow);
    double total_read_data = std::get<0>(total_data);
    double total_written_data = std::get<1>(total_data);

    double io_read_time = total_read_data / (io_read_speed_per_node * (double)num_nodes);
    double compute_time = compute_total_work(workflow) / ((double)num_nodes * (double)num_cores_per_node);
    double io_write_time = total_written_data / (io_write_speed_per_node * (double)num_nodes);

    return std::max<double>(compute_time, io_read_time + io_write_time);
}

double compute_task_makespan(const std::shared_ptr<wrench::WorkflowTask>& task,
                             double io_read_speed_per_node,
                             double io_write_speed_per_node) {
    double makespan = 0;
    for (const auto &f : task->getInputFiles()) {
        makespan += f->getSize() / io_read_speed_per_node;
    }

    makespan += task->getFlops();

    for (const auto &f : task->getOutputFiles()) {
        makespan += f->getSize() / io_write_speed_per_node;
    }
    return makespan;
}

double estimate_makespan_level(std::vector<std::shared_ptr<wrench::WorkflowTask>> tasks,
                               unsigned long num_nodes,
                               unsigned long num_cores_per_node,
                               double io_read_speed_per_node,
                               double io_write_speed_per_node) {

    // Sort the vector of tasks according to task makespans
    std::sort(tasks.begin(), tasks.end(),
              [io_read_speed_per_node, io_write_speed_per_node]
                      (const std::shared_ptr<wrench::WorkflowTask> & a, const std::shared_ptr<wrench::WorkflowTask> & b) -> bool
              {
                  double makespan_a = compute_task_makespan(a, io_read_speed_per_node, io_write_speed_per_node);
                  double makespan_b = compute_task_makespan(b, io_read_speed_per_node, io_write_speed_per_node);
                  return makespan_a > makespan_b;
              });

    // Go through batches of tasks
    double level_makespan = 0.0;
    int num_batches = (int)(std::ceil((double) tasks.size() / ((double)num_nodes * (double)num_cores_per_node)));
    for (int i = 0; i < num_batches; i++) {
        int first_task = i * (int)num_nodes * (int)num_cores_per_node;
        int last_task = std::min<int>((int)tasks.size() - 1, (i+1) * (int)num_nodes * (int)num_cores_per_node - 1);
        int num_tasks = last_task - first_task + 1;
        double io_contention = ((double)num_tasks / (double)num_nodes);
        double sum_task_makespans = 0;
        for (int t = first_task; t <= last_task; t++) {
            sum_task_makespans += compute_task_makespan(tasks.at(t), io_read_speed_per_node / io_contention,
                                                        io_write_speed_per_node / io_contention);
        }
        level_makespan += sum_task_makespans / num_tasks; // average task run time accounting for contention
    }

    return level_makespan;
}

double estimate_makespan_critical_path(const std::shared_ptr<wrench::Workflow>& workflow,
                                       unsigned long num_nodes,
                                       unsigned long num_cores_per_node,
                                       double io_read_speed_per_node,
                                       double io_write_speed_per_node) {

    double makespan = 0.0;
    for (int i = 0; i < workflow->getNumLevels(); i++) {
        makespan += estimate_makespan_level(workflow->getTasksInTopLevelRange(i,i),
                                            num_nodes, num_cores_per_node,
                                            io_read_speed_per_node, io_write_speed_per_node);
    }
    return makespan;
}

/**
 * @brief The main function
 *
 * @param argc: argument count
 * @param argv: argument array
 * @return 0 on success, non-zero otherwise
 */
int main(int argc, char **argv) {

    /* Create a WRENCH simulation object */
    auto simulation = wrench::Simulation::createSimulation();

    /* Initialize the simulation (which there isn't any) */
    simulation->init(&argc, argv);

    std::string workflow_file;
    std::string s_flops_per_unit_of_cpu_work;
    std::string s_task_type;
    int num_cores;

    std::vector<std::string> s_platform_specs;

    /* Parsing of the command-line arguments */
    // Define command-line argument options
    po::options_description desc("Allowed options", 100);
    desc.add_options()
            ("help",
             "Show this help message\n")
            ("workflow", po::value<std::string>(&workflow_file)->required()->value_name("<path>"),
             "Path to JSON workflow description file\n")
            ("platform_spec", po::value<std::vector<std::string>>(&s_platform_specs)->required()->value_name("<cpu_task_exec_time:mem_task_exec_time:per_node_io_read_bw:per_node_io_write_bw:num_cores_per_nodes | name>"),
             "Possible values:\n\t- specific values, e.g., 200:300:100MBps:80kbps:16\n\t- Summit\n\t- Piz Daint\n")
            ("num_cores", po::value<int>(&num_cores)->required()->value_name("<num cores>"),
             "The total number of cores")
            ;

    // Parse command-line arguments
    po::variables_map vm;
    try {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        // Print help message and exit if needed
        if (vm.count("help")) {
            std::cerr << desc << "\n";
            exit(0);
        }
        // Throw whatever exception in case argument values are erroneous
        po::notify(vm);
    } catch (std::exception &e) {
        cerr << "Error: " << e.what() << "\n";
        exit(1);
    }


    /* Create the workflow */
    auto workflow = WfCommonsWorkflowParser::createWorkflowFromJSON(workflow_file, 1.0, false);


    for (auto const &platform_spec : s_platform_specs) {

        unsigned long num_cores_per_node;
        double cpu_task_execution_time;
        double mem_task_execution_time;
        double io_read_speed_per_node;
        double io_write_speed_per_node;

        if (platform_spec.find(':') != string::npos) {
            std::vector<std::string> tokens;
            boost::split(tokens, platform_spec, boost::is_any_of(":"));
            if (tokens.size() != 5) {
                std::cerr << "Error: invalid platform specification " << platform_spec << "\n";
                exit(1);
            }
            cpu_task_execution_time = strtod(tokens.at(0).c_str(), nullptr);
            mem_task_execution_time = strtod(tokens.at(1).c_str(), nullptr);
            io_read_speed_per_node = UnitParser::parse_bandwidth(tokens.at(2));
            io_write_speed_per_node = UnitParser::parse_bandwidth(tokens.at(3));
            num_cores_per_node = strtoul(tokens.at(4).c_str(), nullptr, 10);
        } else if (platform_specs.find(platform_spec) != platform_specs.end()) {
            cpu_task_execution_time = platform_specs[platform_spec].cpu_task_execution_time;
            mem_task_execution_time = platform_specs[platform_spec].mem_task_execution_time;
            io_read_speed_per_node = platform_specs[platform_spec].io_read_speed_per_node;
            io_write_speed_per_node = platform_specs[platform_spec].io_write_speed_per_node;
            num_cores_per_node = platform_specs[platform_spec].num_cores_per_node;
        } else {
            std::cerr << "Error: invalid platform specification " << platform_spec << "\n";
            exit(1);
        }

//        std::map<std::string, double> task_types = {{"cpu", cpu_task_execution_time}, {"mem", mem_task_execution_time}};
        std::map<std::string, double> task_types = {{"cpu", cpu_task_execution_time}};
        for (auto const &tt : task_types) {

            double task_execution_time = tt.second;

            // Set task flops
            for (auto const &t : workflow->getTasks()) {
                t->setFlops(task_execution_time);
            }

            auto total_data = compute_total_data(workflow);
            double total_read_data = std::get<0>(total_data);
            double total_written_data = std::get<1>(total_data);

            unsigned long num_nodes = std::ceil((double)num_cores / (double)num_cores_per_node);

            fprintf(stderr, "PLATFORM %s:\n", platform_spec.c_str());
            fprintf(stderr, "  - %lu %lu-core nodes\n", num_nodes, num_cores_per_node);
            fprintf(stderr, "  - task execution time: %.2lf sec\n", task_execution_time);
            fprintf(stderr, "  - per-node I/O read rate: %.2lf MB/sec\n", io_read_speed_per_node / MBYTE);
            fprintf(stderr, "  - per-node I/O write rate: %.2lf MB/sec\n", io_write_speed_per_node / MBYTE);
            fprintf(stderr, "\nWORKFLOW:\n");
            fprintf(stderr, "  - # TASKS:            %lu\n", workflow->getNumberOfTasks());
            fprintf(stderr, "  - TASK TYPE:          %s\n", tt.first.c_str());
            double total_work = compute_total_work(workflow);
            fprintf(stderr, "  - TOTAL WORK:         %.2lf seconds (%.2lf hours)\n", total_work, total_work / 3600.0);
            fprintf(stderr, "  - TOTAL DATA READ:    %.2lf GB\n", total_read_data / GBYTE);
            fprintf(stderr, "  - TOTAL DATA WRITTEN: %.2lf GB\n", total_written_data / GBYTE);
            double estimate1 = estimate_makespan_naive_no_overlap(workflow, num_nodes, num_cores_per_node,
                                                                  io_read_speed_per_node, io_write_speed_per_node);
            double estimate2 = estimate_makespan_naive_overlap(workflow, num_nodes, num_cores_per_node,
                                                               io_read_speed_per_node, io_write_speed_per_node);
            double estimate3 = estimate_makespan_critical_path(workflow, num_nodes, num_cores_per_node,
                                                               io_read_speed_per_node, io_write_speed_per_node);
            fprintf(stderr, "\nNAIVE / NO CONCURRENCY: %.1lf seconds\n", estimate1);
            fprintf(stderr, "NAIVE / CONCURRENCY   : %.1lf seconds\n", estimate2);
            fprintf(stderr, "CRITICAL PATH         : %.1lf seconds\n", estimate3);

            // Code to print out CSV stuff
            std::vector<std::string> tokens;
            boost::split(tokens, workflow_file, boost::is_any_of("/"));
            std::string after_slash = tokens.at(tokens.size() -1);
            boost::split(tokens, after_slash, boost::is_any_of("-"));
            fprintf(stdout, "\n\nCSV,%s,%s,%s,%s,%.2lf,%.2lf,%.2lf,%s,\n",
                    tokens.at(0).c_str(),
                    tokens.at(1).c_str(),
                    tokens.at(2).c_str(),
                    tt.first.c_str(),
                    estimate1, estimate2, estimate3,
                    platform_spec.c_str());
        }

    }

    return 0;
}
