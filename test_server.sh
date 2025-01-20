#!/bin/bash

# Variables
PORT=8080
BASE_URL="http://localhost:$PORT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m' # No Color

# Function to check the response
check_response() {
    local url=$1
    local expected_status=$2
    local expected_keyword=$3

    echo "Testing $url"
    response=$(curl -i -s $url)
    status=$(echo "$response" | grep HTTP/1.0 | awk '{print $2}')
    if [[ $status -eq $expected_status && "$response" == *"$expected_keyword"* ]]; then
        echo -e "${GREEN}PASS${NC}"
    else
        echo -e "${RED}FAIL${NC}"
        echo "Expected status: $expected_status, Got: $status"
        echo "Expected keyword: $expected_keyword"
        echo "Response: $response"
    fi
    echo
}

# Start the server
./server $PORT 4 10 100 &
SERVER_PID=$!
sleep 2  # Give the server some time to start

# Tests
check_response $BASE_URL 200 "Directory Listing"
check_response "$BASE_URL/existingdirectory/index.html" 200 "<h1>Index File</h1>"
check_response "$BASE_URL/nonexistentfile" 404 "404 Not Found"
check_response "$BASE_URL/existingdirectory/" 200 "Directory Listing"
check_response "$BASE_URL/existingdirectory" 302 "302 Found"
check_response "$BASE_URL/existingdirectory/file.txt" 200 "This is a test file."
check_response "$BASE_URL" 200 "Directory Listing"

# Unsupported method
echo "Testing unsupported method"
response=$(curl -i -s -X POST $BASE_URL)
status=$(echo "$response" | grep HTTP/1.0 | awk '{print $2}')
if [[ $status -eq 501 && "$response" == *"501 Not Implemented"* ]]; then
    echo -e "${GREEN}PASS${NC}"
else
    echo -e "${RED}FAIL${NC}"
    echo "Expected status: 501, Got: $status"
    echo "Response: $response"
fi
echo
sudo apt-get install valgrind

# Stop the server
kill $SERVER_PID