file(READ "${INPUT_FILE}" hex_content HEX)

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " array_content "${hex_content}")

file(WRITE "${OUTPUT_FILE}" "const unsigned char ${VARIABLE_NAME}[] = {\n ${array_content} 0x00 \n};\n")
file(APPEND "${OUTPUT_FILE}" "const unsigned int ${VARIABLE_NAME}_len = sizeof(${VARIABLE_NAME});\n")
