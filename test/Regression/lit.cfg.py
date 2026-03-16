import os
import lit.formats
from dotenv import load_dotenv

# Define config if it's not already provided by the lit runner
if 'config' not in globals():
    class LitConfig:
        def __init__(self):
            self.name = ""
            self.suffixes = []
            self.test_source_root = ""
            self.test_exec_root = ""
            self.substitutions = []
            self.test_format = None
    config = LitConfig()

# Search for .env from current directory or parents
load_dotenv()

config.name = "trsc"
config.suffixes = [".rs", ".mlir"]
config.test_format = lit.formats.ShTest(True)

# Use environment variables directly (no os.path)
config.test_source_root = os.getenv("TEST_SOURCE_ROOT")
config.test_exec_root = os.getenv("TEST_EXEC_ROOT")

# substitutions
config.substitutions.append(("%trsc", os.getenv("TRSC_BIN")))
config.substitutions.append(("%trsc-opt", os.getenv("TRSC_OPT_BIN")))
config.substitutions.append(("%FileCheck", os.getenv("FILECHECK_BIN")))
config.substitutions.append(("%not", os.getenv("NOT_BIN")))
