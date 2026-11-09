execute_process(COMMAND "${EDITOR}" --rhi-smoke-test
    WORKING_DIRECTORY "${SOURCE_DIR}" TIMEOUT 40
    RESULT_VARIABLE RESULT OUTPUT_VARIABLE OUTPUT ERROR_VARIABLE ERRORS)
file(WRITE "${LOG_DIR}/rhi-smoke.log" "${OUTPUT}")
file(WRITE "${LOG_DIR}/rhi-smoke-errors.log" "${ERRORS}")
if(NOT RESULT EQUAL 0 OR "${OUTPUT}\n${ERRORS}" MATCHES "(ERROR|WARNING|\\[Error\\]|\\[Warning\\]|Validation Error)")
    message(FATAL_ERROR "RHI smoke test failed (${RESULT}):\n${OUTPUT}\n${ERRORS}")
endif()
message(STATUS "${OUTPUT}\n${ERRORS}")
