#!/bin/sh

printf "Normal:       The quick brown fox jumps over the lazy dog.\n"
printf "\033[1mBold:         The quick brown fox jumps over the lazy dog.\033[0m\n"
printf "\033[3mItalic:       The quick brown fox jumps over the lazy dog.\033[0m\n"
printf "\033[4mUnderline:    The quick brown fox jumps over the lazy dog.\033[0m\n"
printf "\033[1;3mBold+Italic:  The quick brown fox jumps over the lazy dog.\033[0m\n"
printf "\033[1;4mBold+Under:   The quick brown fox jumps over the lazy dog.\033[0m\n"
printf "\033[3;4mItalic+Under: The quick brown fox jumps over the lazy dog.\033[0m\n"
printf "\033[1;3;4mAll Three:    The quick brown fox jumps over the lazy dog.\033[0m\n"
