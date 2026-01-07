#!/bin/sh

# Clear the screen
clear

echo "Dash Terminal Theme Test"

echo "1. Standard Colors (Background)"
for i in 0 1 2 3 4 5 6 7; do
    printf "\033[4${i}m  %2d  \033[0m" $i
done
echo ""
# High intensity backgrounds (100-107)
for i in 0 1 2 3 4 5 6 7; do
    val=`expr $i + 8`
    printf "\033[10${i}m  %2d  \033[0m" $val
done
echo ""
echo ""

echo "2. Attributes & Colors (Foreground)"
printf "%-10s %-10s %-10s %-10s %-10s %-10s\n" "Normal" "Bold" "Italic" "Underline" "Blink" "Inverse"

for i in 0 1 2 3 4 5 6 7; do
    # Normal
    printf "\033[3${i}m%-10s\033[0m " "Color $i"
    # Bold
    printf "\033[1;3${i}m%-10s\033[0m " "Bold"
    # Italic
    printf "\033[3;3${i}m%-10s\033[0m " "Italic"
    # Underline
    printf "\033[4;3${i}m%-10s\033[0m " "Under"
    # Blink
    printf "\033[5;3${i}m%-10s\033[0m " "Blink"
    # Inverse
    printf "\033[7;3${i}m%-10s\033[0m " "Inverse"
    echo ""
done
# High intensity foregrounds
for i in 0 1 2 3 4 5 6 7; do
    # Normal (90-97)
    val=`expr $i + 8`
    printf "\033[9${i}m%-10s\033[0m " "Color $val"
    # Bold
    printf "\033[1;9${i}m%-10s\033[0m " "Bold"
    # Italic
    printf "\033[3;9${i}m%-10s\033[0m " "Italic"
    # Underline
    printf "\033[4;9${i}m%-10s\033[0m " "Under"
    # Blink
    printf "\033[5;9${i}m%-10s\033[0m " "Blink"
    # Inverse
    printf "\033[7;9${i}m%-10s\033[0m " "Inverse"
    echo ""
done
echo ""

echo "3. Attribute Combinations"
printf "\033[1;4mBold + Underline\033[0m\n"
printf "\033[1;3mBold + Italic\033[0m\n"
printf "\033[3;4mItalic + Underline\033[0m\n"
printf "\033[1;3;4mBold + Italic + Underline\033[0m\n"
printf "\033[1;3;4;7mBold + Italic + Underline + Inverse\033[0m\n"
echo ""

echo "4. Box Drawing (UTF-8)"
echo "┌─┬─┐  ╔═╦═╗"
echo "│ │ │  ║ ║ ║"
echo "├─┼─┤  ╠═╬═╣"
echo "│ │ │  ║ ║ ║"
echo "└─┴─┘  ╚═╩═╝"
echo ""