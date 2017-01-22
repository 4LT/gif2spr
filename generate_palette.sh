#!/bin/sh
xxd -i quakepal \
    | sed 's/\(^.*\)quakepal/static \1 const DEFPAL/' \
    | sed 's/^unsigned\ int.*//' \
    | cat defpal.h_head - defpal.h_foot \
    > defpal.h
