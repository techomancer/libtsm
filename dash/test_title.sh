#!/bin/sh

printf "\033]0;Test Title 1\007"
echo "Set title to 'Test Title 1'"
sleep 1
printf "\033]2;Test Title 2\007"
echo "Set title to 'Test Title 2'"
