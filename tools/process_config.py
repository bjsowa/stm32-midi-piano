#!/usr/bin/python3

import sys
from os import path
import yaml

print(sys.version)

if len(sys.argv) < 2:
    raise Exception("Not enough arguments! Usage: process_config.py [path]")

project_path = sys.argv[1]

config_yaml_path = path.join(project_path, "config.yaml")
config_header_path = path.join(project_path, "include/config.h")

yaml_file = open(config_yaml_path, 'r')
config = yaml.full_load(yaml_file)

print(config)