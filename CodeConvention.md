Error handling policy
 - non-recoverable faults - throw an exception (SE_VERIFY)
 - recoverable faults - use error codes (or SE_ENSURE to attach a debugger and log the error)
 - no exceptions below rhi level (logging, utils, configs, memory allocations, etc)

Code style
 - underscore_local_variable
 - m_underscore_class_data_member
 - underscore_function_parameters
 - append units of measurement suffix when applicable, eg. frametime_ms instead of frametime
 - _b can be omitted for memory size variables (but not _kb or _mb)
 - CamelCaseForFunctionNames()
 - class CamelCaseForClassNames
 - CamelCaseForFileNames.ext
 - CamelCaseForFolderNames/SomeFile.ext
 - 4 spaces instead of tabs

Misc
 - modules have a flat folder structure
 - use #include <something> for communication between modules and for 3rdparty libs
 - use only #include "something" inside a module
