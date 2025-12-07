cmake_minimum_required(VERSION 3.10.0)
# set(EXTRA_COMPONENT_DIRS "../../")  # Remove or correct this line if necessary.
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(omniframe-esp)

idf_build_set_property(PARTITION_TABLE_FILENAME "partitions.csv" APPEND)