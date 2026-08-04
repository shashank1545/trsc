import os
import lit.formats

config.name = "trsc"
config.suffixes = [".rs", ".mlir"]
config.test_format = lit.formats.ShTest(True)
config.environment["PATH"] = (
    os.path.dirname(config.not_bin)
    + os.pathsep
    + os.environ.get("PATH", "")
)

# substitutions
config.substitutions.append(("%trsc-opt", config.trsc_opt_bin))
config.substitutions.append(("%trsc", config.trsc_bin))
config.substitutions.append(("%FileCheck", config.filecheck_bin))
config.substitutions.append(("%not", config.not_bin))
