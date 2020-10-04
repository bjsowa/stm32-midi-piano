import sys

Import("env")

env.Execute("$PYTHONEXE -m pip install pyyaml")
return_code = env.Execute("$PYTHONEXE $PROJECT_DIR/tools/process_config.py $PROJECT_DIR")

sys.exit(return_code)