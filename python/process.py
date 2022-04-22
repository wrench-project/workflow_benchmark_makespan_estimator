#!/usr/bin/env python3 
import sys
import matplotlib.pyplot as plt
from matplotlib.pyplot import figure
import itertools

db = []
l_machine = []
l_app = []
l_num_tasks = []
l_data_size = []
l_type = []


##
# Construct result DB based on CSV result files
def construct_db(real_results_file, model_results_file):
    global db
    global l_machine
    global l_app
    global l_num_tasks
    global l_data_size
    global l_type

    with open(real_results_file) as f:
        real_lines = f.readlines()

    with open(model_results_file) as f:
        model_lines = f.readlines()

    # Build dictionary of results
    for line in real_lines:
        if line.startswith("app"):
            continue
        tokens = line.strip().split(",")
        if tokens[4] == "":
            continue

        data = {"app": tokens[0],
                "num_tasks": int(tokens[1]),
                "data_size": int(tokens[2]),
                "type": tokens[3],
                "machine": tokens[5],
                "real": float(tokens[4]),
                "estimate1": 0.0,
                "estimate2": 0.0,
                "estimate3": 0.0}

        db.append(data)
        l_machine.append(data["machine"])
        l_app.append(data["app"])
        l_num_tasks.append(data["num_tasks"])
        l_data_size.append(data["data_size"])
        l_type.append(data["type"])

    l_machine = list(set(l_machine))
    l_app = list(set(l_app))
    l_num_tasks = list(set(l_num_tasks))
    l_data_size = list(set(l_data_size))
    l_type = list(set(l_type))

    for line in model_lines:
        tokens = line.strip().split(",")
        data = {"app": tokens[0],
                "num_tasks": int(tokens[1]),
                "data_size": int(tokens[2]),
                "type": tokens[3],
                "estimate1": float(tokens[4]),
                "estimate2": float(tokens[5]),
                "estimate3": float(tokens[6]),
                "machine": tokens[7]}

        keys = ["app", "num_tasks", "data_size", "type", "machine"]
        for i in range(0, len(db)):
            to_update = True
            for k in keys:
                if db[i][k] != data[k]:
                    to_update = False
                    break
            if to_update:
                db[i]["estimate1"] = data["estimate1"]
                db[i]["estimate2"] = data["estimate2"]
                db[i]["estimate3"] = data["estimate3"]
                break

    # # remove incomplete entries
    # cleanedup = []
    # for data in db:
    #     if data["estimate1"] != 0:
    #         cleanedup.append(data)
    # db = cleanedup
    return


def get_results(query):
    for data in db:
        match = True
        for k in query.keys():
            if data[k] != query[k]:
                match = False
                break
        if match:
            return [data["real"], data["estimate1"], data["estimate2"], data["estimate3"]]
    raise "Oh no!"


def print_ranking_mistakes():

    machine_pairs = list(itertools.combinations(l_machine, 2))

    estimate1_wrong = 0
    estimate2_wrong = 0
    estimate3_wrong = 0
    estimate1_right = 0
    estimate2_right = 0
    estimate3_right = 0

    for machine_pair in machine_pairs:
        machine1 = machine_pair[0]
        machine2 = machine_pair[1]
        print("* COMPARISON " + machine1 + " vs. " + machine2)
        for app in l_app:
            print("  * APP: " + app)
            for num_tasks in l_num_tasks:
                print("    * NUM_TASKS: " + str(num_tasks))
                for data_size in l_data_size:
                    print("      * DATA_SIZE: " + str(data_size))
                    for t in l_type:
                        print("        * TYPE: " + t)
                        try:
                            [real_machine1, estimate1_machine1, estimate2_machine1, estimate3_machine1] = get_results(
                                {"app": app, "num_tasks": num_tasks, "data_size": data_size, "type": t, "machine": machine1})
                            [real_machine2, estimate1_machine2, estimate2_machine2, estimate3_machine2] = get_results(
                                {"app": app, "num_tasks": num_tasks, "data_size": data_size, "type": t, "machine": machine2})
                        except:
                            print("           NO RESULTS")
                            continue
                        if (estimate1_machine1 <= 0) or (estimate1_machine2 <= 0):
                            continue
                        real_faster = real_machine1 < real_machine2
                        estimate1_faster = estimate1_machine1 < estimate1_machine2
                        estimate2_faster = estimate2_machine1 < estimate2_machine2
                        estimate3_faster = estimate3_machine1 < estimate3_machine2
                        print("          * ESTIMATE1 CORRECT: " + str(estimate1_faster == real_faster))
                        print("          * ESTIMATE2 CORRECT: " + str(estimate2_faster == real_faster))
                        print("          * ESTIMATE3 CORRECT: " + str(estimate3_faster == real_faster))
                        estimate1_wrong += estimate1_faster != real_faster
                        estimate2_wrong += estimate2_faster != real_faster
                        estimate3_wrong += estimate3_faster != real_faster
                        estimate1_right += estimate1_faster == real_faster
                        estimate2_right += estimate2_faster == real_faster
                        estimate3_right += estimate3_faster == real_faster

    print("ESTIMATE 1: WRONG " + str(estimate1_wrong) + "  RIGHT " + str(estimate1_right))
    print("ESTIMATE 2: WRONG " + str(estimate2_wrong) + "  RIGHT " + str(estimate2_right))
    print("ESTIMATE 3: WRONG " + str(estimate3_wrong) + "  RIGHT " + str(estimate3_right))


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("Usage: " + sys.argv[0] + " <real.csv> <model.csv>\n")
        sys.exit(1)

    # Construct DB of results
    construct_db(sys.argv[1], sys.argv[2])

    # Print Ranking Mistakes
    print_ranking_mistakes()

# x = list(range(0,len(real)))
#
# bar_width=0.2
#
# plt.bar(x, real, color='b', width=bar_width, label="real")
# plt.bar([t+bar_width for t in x], estimate1, color='k', width=bar_width, label="estimate1")
# #plt.bar([t+2*bar_width for t in x], estimate4, color='g', width=bar_width, label="estimate4")
# plt.bar([t+2*bar_width for t in x], estimate2, color='g', width=bar_width, label="estimate2")
# plt.bar([t+3*bar_width for t in x], estimate3, color='r', width=bar_width, label="estimate3")
#
# plt.xticks(x, keys, rotation=-90)
#
# plt.legend()
#
# plt.tight_layout()
# plt.savefig("plot.pdf")
# plt.close()


if __name__ == "__main__":
    main()
