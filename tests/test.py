#!/usr/bin/env python3

import os
import subprocess
import glob
import datetime

if __name__ == "__main__":
    # paths
    runtime_path = "../src/debug/libruntime.so"
    header_dir = "../src/"
    bin_dir = "./bin"
    log_dir = "./log"
    os.makedirs(bin_dir, exist_ok=True)
    os.makedirs(log_dir, exist_ok=True)

    timestamp = datetime.datetime.now().strftime("%m-%d-%H-%M")
    outlog_path = os.path.join(log_dir, "log-" + timestamp + ".out")
    errlog_path = os.path.join(log_dir, "log-" + timestamp + ".err")

    try:
        outlog_file = open(outlog_path, mode="w")
    except OSError as e:
        print(f"ERROR: unable to open log file {outlog_path}: {e}")
        exit(1)
    try:
        errlog_file = open(errlog_path, mode="w")
    except OSError as e:
        print(f"ERROR: unable to open log file {errlog_path}: {e}")
        exit(1)

    test_inputs = {
        "test3": "auth admin\nreset\nservice " + "A" * 28 + "\nlogin\n",
    }

    total_cnt = 0
    total_success = 0

    test_files = glob.glob("./src/test*.c")
    test_files.sort()

    for test_file in test_files:
        # remove .c extension
        test_name = os.path.splitext(os.path.basename(test_file))[0]
        binary_path = os.path.join(bin_dir, test_name)

        # compile the test case
        print(f"INFO: compile {test_file} -> {binary_path}")
        compile_cmd = ["clang", "-ggdb3", test_file, "-I" + header_dir,
                       "-o", binary_path]
        result = subprocess.run(compile_cmd, capture_output=True, text=True)

        if result.returncode != 0:
            print(f"ERROR: compilation failed for {
                  test_file}:\n{result.stderr}")
            continue

        # run the test case
        print(f"INFO: running {test_name}...")
        env = os.environ.copy()
        env["LD_PRELOAD"] = runtime_path

        input_data = test_inputs.get(test_name, None)  # get input if defined
        # need to fflush stdout of target file to fully capture output
        ps = subprocess.run(binary_path,
                            input=input_data, text=True, env=env,
                            # stdout=outlog_file, stderr=errlog_file)
                            capture_output=True)

        outlog_file.write("#"*20 + " " + test_name + " " + "#"*20 + "\n")
        outlog_file.write(ps.stdout)
        errlog_file.write("#"*20 + " " + test_name + " " + "#"*20 + "\n")
        errlog_file.write(ps.stderr)

        SIGSEGV = -11  # returncode has negative value for signals
        uaf_msg = "REPORT_UAF_OCCURED_REPORT"
        if ps.returncode == SIGSEGV or uaf_msg in ps.stderr:
            print(f"INFO: FAILURE {test_name}")
        elif ps.returncode == 0:
            print(f"INFO: SUCCESS {test_name}")
            total_success += 1
        else:
            print(f"ERROR: exited abnormally with code: {ps.returncode}")
        total_cnt += 1

    print(f"INFO: {total_success}/{total_cnt} tests passed")
    print(f"INFO: find log files: {os.path.relpath(
        outlog_path)} {os.path.relpath(errlog_path)}")
    errlog_file.close()
    outlog_file.close()
