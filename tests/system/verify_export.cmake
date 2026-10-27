if(NOT DEFINED CLI OR NOT DEFINED SCENARIO OR NOT DEFINED OUTPUT_DIR)
    message(FATAL_ERROR "Missing CLI, SCENARIO or OUTPUT_DIR")
endif()

set(json "${OUTPUT_DIR}/cli-export-smoke.json")
set(csv "${OUTPUT_DIR}/cli-export-smoke.csv")
set(paused "${OUTPUT_DIR}/cli-export-paused.json")

execute_process(
    COMMAND "${CLI}" --scenario "${SCENARIO}" --policy rr --quantum 2
            --export-json "${json}" --export-csv "${csv}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Export CLI failed (${rc}): ${error}")
endif()
file(READ "${json}" json_text)
file(READ "${csv}" csv_text)

foreach(required IN ITEMS
        "\"schema\":\"os-llm-run\"" "\"seed\":42"
        "\"finished\":true" "\"trace_fingerprint\""
        "\"input\":[" "\"events\":[" "\"metrics\":{")
    string(FIND "${json_text}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "JSON export missing ${required}")
    endif()
endforeach()
foreach(required IN ITEMS
        "section,index,field,value" "meta,,seed,42"
        "config,,policy,Round-Robin" "event,0,sequence,0"
        "metric,,completed,4")
    string(FIND "${csv_text}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "CSV export missing ${required}")
    endif()
endforeach()

execute_process(
    COMMAND "${CLI}" --steps 0 --export-json "${paused}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error
)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "Paused export CLI failed (${rc}): ${error}")
endif()
file(READ "${paused}" paused_text)
foreach(required IN ITEMS
        "\"finished\":false" "\"seed\":null"
        "\"first_run_tick\":null" "\"events\":[]")
    string(FIND "${paused_text}" "${required}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Paused JSON missing ${required}")
    endif()
endforeach()

file(SHA256 "${SCENARIO}" scenario_before)
execute_process(
    COMMAND "${CLI}" --scenario "${SCENARIO}" --export-json "${SCENARIO}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error
)
if(rc EQUAL 0)
    message(FATAL_ERROR "Export to the input scenario should be rejected")
endif()
file(SHA256 "${SCENARIO}" scenario_after)
if(NOT scenario_before STREQUAL scenario_after)
    message(FATAL_ERROR "Rejected export modified the input scenario")
endif()

file(SHA256 "${json}" json_before)
execute_process(
    COMMAND "${CLI}" --export-json "${json}" --export-csv "${OUTPUT_DIR}/./cli-export-smoke.json"
    RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE error
)
if(rc EQUAL 0)
    message(FATAL_ERROR "Aliased JSON and CSV output paths should be rejected")
endif()
file(SHA256 "${json}" json_after)
if(NOT json_before STREQUAL json_after)
    message(FATAL_ERROR "Rejected aliased export modified the existing output")
endif()
file(REMOVE "${json}" "${csv}" "${paused}")
