#!/bin/bash
source configs/run.env
shopt -s globstar

lockdir="/tmp/mcsp.lock"

write_output() {
  if mkdir "$lockdir" 2> /dev/null; then
    echo "$1" >> "$2"
    rm -rf "$lockdir"
    return 0
  else
    return 1
  fi
}

task() {
  out=$(./bin/run.o ${MCSP_HEURISTIC} ./${1} ./${2} -l -q -t ${TIMEOUT})
  out="${1}, ${2}, ${out}"

  until write_output "$out" "$3"; do
    sleep 0.1  # Small delay before retrying to avoid busy-waiting
  done
}

# Files to process
files=("BI" "CV" "LV" "SI")

# Function to wait for available slot
wait_for_slot() {
  while [ "$(jobs -r | wc -l)" -ge "$N_PARALLEL" ]; do
    sleep 1
  done
}

# Iterate over the files
for f in "${files[@]}"; do
  file_name=indicators/${f}.txt
  results_file_name=results/${f}_${MCSP_HEURISTIC}_${TIMEOUT}.txt

  # Find the number of lines already processed
  processed_lines=0
  if [ -e "$results_file_name" ]; then
    processed_lines=$(wc -l < "$results_file_name")
  fi

  current_line=0
  while IFS=',' read -r file1 file2; do
    current_line=$((current_line + 1))

    if [ "$current_line" -le "$processed_lines" ]; then
      continue  # Skip already processed lines
    fi

    wait_for_slot
    task $file1 $file2 $results_file_name &

  done < "$file_name"
done

# Wait for all tasks to complete
wait
