
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
    double compute_speed_per_core;
    double io_read_speed_per_node;
    double io_write_speed_per_node;
};

std::map<std::string, struct platform_spec> platform_specs = {
        { "summit",
          {148600.0 * TFLOP / 2414592,
           1788.32 * GBYTE / 504,
           2158.70 * GBYTE / 504}
        }
};

double compute_total_flops(std::shared_ptr<wrench::Workflow> workflow) {
    double total_flops = 0.0;
    for (auto const &t : workflow->getTasks()) {
        total_flops += t->getFlops();
    }
    return total_flops;
}

std::pair<double, double> compute_total_data(std::shared_ptr<wrench::Workflow> workflow) {
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

double estimate_makespan_naive_no_overlap(std::shared_ptr<wrench::Workflow> workflow,
                                          int num_nodes,
                                          int num_cores_per_node,
                                          double compute_speed_per_core,
                                          double io_read_speed_per_node,
                                          double io_write_speed_per_node) {

    auto total_data = compute_total_data(workflow);
    double total_read_data = std::get<0>(total_data);
    double total_written_data = std::get<1>(total_data);

    double io_read_time = total_read_data / (io_read_speed_per_node * num_nodes);
    double compute_time = compute_total_flops(workflow) / (num_nodes * num_cores_per_node * compute_speed_per_core);
    double io_write_time = total_written_data / (io_write_speed_per_node * num_nodes);

    return io_read_time + compute_time + io_write_time;
}

double estimate_makespan_naive_overlap(std::shared_ptr<wrench::Workflow> workflow,
                                       int num_nodes,
                                       int num_cores_per_node,
                                       double compute_speed_per_core,
                                       double io_read_speed_per_node,
                                       double io_write_speed_per_node) {

    auto total_data = compute_total_data(workflow);
    double total_read_data = std::get<0>(total_data);
    double total_written_data = std::get<1>(total_data);

    double io_read_time = total_read_data / (io_read_speed_per_node * num_nodes);
    double compute_time = compute_total_flops(workflow) / (num_nodes * num_cores_per_node * compute_speed_per_core);
    double io_write_time = total_written_data / (io_write_speed_per_node * num_nodes);

    return std::max<double>(compute_time, io_read_time + io_write_time);
}

double estimate_makespan_critical_path(std::shared_ptr<wrench::Workflow> workflow,
                                       int num_nodes,
                                       int num_cores_per_node,
                                       double compute_speed_per_core,
                                       double io_read_speed_per_node,
                                       double io_write_speed_per_node) {

    return -1.0;
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
    std::string platform_spec;
    std::string s_flops_per_unit_of_cpu_work;
    int num_nodes;
    int num_cores_per_node;

    /* Parsing of the command-line arguments */
    // Define command-line argument options
    po::options_description desc("Allowed options", 100);
    desc.add_options()
            ("help",
             "Show this help message\n")
            ("workflow", po::value<std::string>(&workflow_file)->required()->value_name("<path>"),
             "Path to JSON workflow description file\n")
            ("flops_per_unit_of_cpu_work", po::value<std::string>(&s_flops_per_unit_of_cpu_work)->required()->value_name("<flops per unit of cpu work>"),
             "Number of flops per unit of CPU work passed to the workflow task benchmark (e.g., \"100Gf\")\n")
            ("platform_spec", po::value<std::string>(&platform_spec)->required()->value_name("<per_core_flops:per_node_io_read_bw:per_node_io_write_bw | name>"),
             "Possible values:\n\t- specific values, e.g., 200Gf:100MBps:80kbps\n\t- summit (values are from Top500 and IO500)\n")
            ("num_nodes", po::value<int>(&num_nodes)->required()->value_name("<num nodes>"),
             "The number of compute nodes used for running the workflow")
            ("num_cores_per_node", po::value<int>(&num_cores_per_node)->required()->value_name("<num cores per node>"),
             "The number of cores per compute node")
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

    double flops_per_unit_of_cpu_work = UnitParser::parse_compute_speed(s_flops_per_unit_of_cpu_work);

    double compute_speed_per_core;
    double io_read_speed_per_node;
    double io_write_speed_per_node;

    if (platform_spec.find(':') != string::npos) {
        std::vector<std::string> tokens;
        boost::split(tokens, platform_spec, boost::is_any_of(":"));
        if (tokens.size() != 3) {
            std::cerr << "Error: invalid platform specification " << platform_spec << "\n";
            exit(1);
        }
        compute_speed_per_core = UnitParser::parse_compute_speed(tokens.at(0));
        io_read_speed_per_node = UnitParser::parse_bandwidth(tokens.at(1));
        io_write_speed_per_node = UnitParser::parse_bandwidth(tokens.at(2));
    } else if (platform_specs.find(platform_spec) != platform_specs.end()) {
        compute_speed_per_core = platform_specs[platform_spec].compute_speed_per_core;
        io_read_speed_per_node = platform_specs[platform_spec].io_read_speed_per_node;
        io_write_speed_per_node = platform_specs[platform_spec].io_write_speed_per_node;
    } else {
        std::cerr << "Error: invalid platform specification " << platform_spec << "\n";
        exit(1);
    }

    /* Create the workflow */
    auto workflow = WfCommonsWorkflowParser::createWorkflowFromJSON(workflow_file, flops_per_unit_of_cpu_work, false);

    auto total_data = compute_total_data(workflow);
    double total_read_data = std::get<0>(total_data);
    double total_written_data = std::get<1>(total_data);

    fprintf(stderr, "PLATFORM:\n");
    fprintf(stderr, "  - %d %d-core nodes\n", num_nodes, num_cores_per_node);
    fprintf(stderr, "  - core flop rate: %.2lf Gflop/sec\n", compute_speed_per_core / GFLOP);
    fprintf(stderr, "  - per-node I/O read rate: %.2lf GB/sec\n", io_read_speed_per_node / GBYTE);
    fprintf(stderr, "  - per-node I/O write rate: %.2lf GB/sec\n", io_write_speed_per_node / GBYTE);
    fprintf(stderr, "\nWORKFLOW:\n");
    fprintf(stderr, "  - TOTAL WORK:         %.2lf Tflop\n", compute_total_flops(workflow) / TFLOP);
    fprintf(stderr, "  - TOTAL DATA READ:    %.2lf GB\n", total_read_data / GBYTE);
    fprintf(stderr, "  - TOTAL DATA WRITTEN: %.2lf GB\n", total_written_data / GBYTE);
    fprintf(stderr, "\nNAIVE / NO CONCURRENCY: %.2lf hours\n", estimate_makespan_naive_no_overlap(workflow, num_nodes, num_cores_per_node,
                                                                                                compute_speed_per_core, io_read_speed_per_node, io_write_speed_per_node) / 3600);
    fprintf(stderr, "\nNAIVE / CONCURRENCY   : %.2lf hours\n", estimate_makespan_naive_overlap(workflow, num_nodes, num_cores_per_node,
                                                                                             compute_speed_per_core, io_read_speed_per_node, io_write_speed_per_node) / 3600);
    fprintf(stderr, "\nCRITICAL PATH         : %.2lf hours\n", estimate_makespan_critical_path(workflow, num_nodes, num_cores_per_node,
                                                                                             compute_speed_per_core, io_read_speed_per_node, io_write_speed_per_node) / 3600);


    return 0;
}