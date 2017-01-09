#!/usr/bin/env python
"""
 *
 * Copyright (c) 2016 Cisco Systems, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   Neither the name of the Cisco Systems, Inc. nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
"""

import os
import sys
import subprocess
import time
import logging
import argparse
import json
import gzip


def end_process(process):
    """
    Takes care of the end-of-life stage of a process.
    If the process is still running, end it.
    The process EOL return code is collected and passed back.
    :param process: A python subprocess object, i.e. subprocess.Popen()
    :return: 0 for process success
    """
    if process.poll() is None:
        # Gracefully terminate the process
        process.terminate()
        time.sleep(1)
        if process.poll() is None:
            # Hard kill the process
            process.kill()
            time.sleep(1)
            if process.poll() is None:
                # Runaway zombie process
                logger.error('subprocess ' + str(process) + 'turned zombie')
                return 1
    elif process.poll() != 0:
        # Export process ended with bad exit code
        return process.poll()

    return 0


def intraop_export_to_collect(cli_paths, flows):
    collect_output = 'tmp-ipfix-collect.json.gz'
    export_output = 'tmp-ipfix-export.json.gz'
    exec_path = cli_paths['exec_path']
    pcap_path = cli_paths['pcap_path']
    rc_overall = 0

    # Start the ipfix collector
    proc_collect = subprocess.Popen([exec_path,
                                     'output=' + collect_output,
                                     'ipfix_collect_online=1',
                                     'ipfix_collect_port=4739'])
    time.sleep(1)

    # Start the ipfix exporter
    proc_export = subprocess.Popen([exec_path,
                                    'output=' + export_output,
                                    'ipfix_export_port=2000',
                                    pcap_path])
    proc_export.wait()
    time.sleep(1)

    """
    Cleanup
    """
    # End the ipfix exporting
    rc_test = end_process(proc_export)
    if rc_test != 0:
        rc_overall = rc_test

    # End the ipfix collecting
    rc_test = end_process(proc_collect)
    if rc_test != 0:
        rc_overall = rc_test

    with gzip.open(collect_output, 'r') as f:
        for line in f:
            try:
                flow = json.loads(line)
                flows.append(flow)
            except:
                continue

    # Delete temporary files
    if os.path.isfile(collect_output):
        os.remove(collect_output)
    if os.path.isfile(export_output):
        os.remove(export_output)

    return rc_overall


def sniff_pcap(cli_paths, flows):
    collect_output = 'tmp-ipfix-collect.json.gz'
    exec_path = cli_paths['exec_path']
    pcap_path = cli_paths['pcap_path']
    rc_overall = 0

    # Start the ipfix collector
    proc_collect = subprocess.Popen([exec_path,
                                     'output=' + collect_output,
                                     'ipfix_collect_port=4739',
                                     pcap_path])
    time.sleep(1)

    """
    Cleanup
    """
    # End the ipfix collecting
    rc_test = end_process(proc_collect)
    if rc_test != 0:
        rc_overall = rc_test

    with gzip.open(collect_output, 'r') as f:
        for line in f:
            try:
                flow = json.loads(line)
                flows.append(flow)
            except:
                continue

    # Delete temporary files
    if os.path.isfile(collect_output):
        os.remove(collect_output)

    return rc_overall


def validate_export_against_sniff(cli_paths):
    exported_flows = list()
    sniff_flows = list()
    compare_keys = ['sa', 'da', 'sp', 'dp', 'pr']

    rc_validate = intraop_export_to_collect(cli_paths=cli_paths,
                                            flows=exported_flows)
    if rc_validate != 0:
        logger.warning(str(intraop_export_to_collect) + 'failed')
        return rc_validate

    rc_validate = sniff_pcap(cli_paths=cli_paths,
                             flows=sniff_flows)
    if rc_validate != 0:
        logger.warning(str(sniff_pcap) + 'failed')
        return rc_validate

    invalid_flows = list()
    for flow in exported_flows:
        corrupt = True
        if not 'sa' in flow:
            # Optimize prelim check to see if a flow object
            continue
        elif flow['dp'] == 4739:
            # Ignore the exporter -> collector initial packet
            continue

        for sniff_flow in sniff_flows:
            if not 'sa' in sniff_flow:
                # Optimize prelim check to see if a flow object
                continue

            match = True
            for key in compare_keys:
                try:
                    if not flow[key] == sniff_flow[key]:
                        # One of the key/value pairs did not match
                        match = False
                        break
                except KeyError:
                    # This json object is not a flow, skip
                    break

            if match is True:
                # All of the key/value pairs matched
                corrupt = False
                break

        if corrupt is True:
            invalid_flows.append(flow)
            rc_validate = 1

    if invalid_flows:
        # Provide the corrupt flows for info purpose
        for flow in invalid_flows:
            logger.info('CORRUPT FLOW: ' + str(flow))

    return rc_validate


def test_unix_os():
    """
    Prepare the module for testing within a UNIX-like enviroment,
    and then run the appropriate test functions.
    :return: 0 for success
    """
    rc_unix_overall = 0
    cur_dir = os.path.dirname(__file__)

    cli_paths = dict()
    cli_paths['exec_path'] = os.path.join(cur_dir, '../bin/pcap2flow')
    cli_paths['pcap_path'] = os.path.join(cur_dir, '../sample.pcap')

    rc_unix_test = validate_export_against_sniff(cli_paths)
    if rc_unix_test != 0:
        rc_unix_overall = rc_unix_test
        logger.warning(str(validate_export_against_sniff) +
                       ' failed with return code ' + str(rc_unix_test))

    return rc_unix_overall


def main():
    """
    Main function to run any test within module.
    :return: 0 for success
    """
    global logger
    logger = logging.getLogger(__name__)

    os_platform = sys.platform
    unix_platforms = ['linux', 'linux2', 'darwin']

    if os_platform in unix_platforms:
        status = test_unix_os()
        if status is not 0:
            logger.warning('FAILED')
            return status

    logger.warning('SUCCESS')
    return 0


if __name__ == "__main__":
    """
    test_ipfix.py executing through shell
    """
    parser = argparse.ArgumentParser(
        description='Joy IPFix program execution tests'
    )
    parser.add_argument('-l', '--log',
                        dest='log_level',
                        choices=['debug', 'info', 'warning', 'error', 'critical'],
                        help='Set the logging level')
    args = parser.parse_args()

    # Configure root logging
    if args.log_level:
        logging.basicConfig(level=args.log_level.upper())
    else:
        logging.basicConfig()

    rc_main = main()
    exit(rc_main)
