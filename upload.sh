#!/bin/bash
echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
echo "Upload (a.k.a flashing) via ESP IDF (idf.py)"
echo ""

# Define the valid values
valid_values=("d1" "D1" "c3" "C3" "s3" "S3")
boards=("esp32" "esp32" "esp32c3" "esp32c3" "esp32s3" "esp32s3")

# Function to check if the input is a valid value
is_valid_value() {
    for val in "${valid_values[@]}"; do
        if [ "$val" == "$1" ]; then
            return 0  # Valid value
        fi
    done
    return 1  # Invalid value
}

# Check if a parameter is provided
if [ $# -eq 1 ] && is_valid_value "$1"; then
    echo "Selected parameter: $1"
    echo ""
else
    echo "Invalid parameter. Please choose from the following options:"
    echo ""
    for ((i=1; i < (${#valid_values[@]}); i = i + 2)); do
	    echo "$((i+1)): ${valid_values[i]}"
	done
	echo ""

    # Prompt user to choose by entering the numeric index
    read -p "Enter the numeric index of the desired parameter: " choice
    if [ "$choice" -ge 1 ] && [ "$choice" -le "${#valid_values[@]}" ]; then
        selected_board="${boards[($choice - 1)]}"
        echo ""
        echo "Selected BOARD chip: $selected_board"
        echo ""
    else
    	echo ""
        echo "Invalid choice. Exiting."
        echo ""
        exit 1
    fi
fi

idf.py --version

PORT="/dev/cu.usbmodem2101"  # Change this to your board's USB port

# Set up ESP-IDF environment
# source "$IDF_PATH/export.sh"

# Build and upload the code
echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
echo "         List of serial devices."
echo ""
ls -l /dev/cu.*
echo ""
echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
read -p "         Is the > ${PORT} < listed? (y/n, default: n): " choice
case "$choice" in
    [yY])
        echo "OK."
        ;;
    *)
		echo ""
        echo "Plug the Borad via USB. Exiting..."
        echo "If so maybe you need enter 'USB Mode' on sone boards. Holding the 'Secondary' button and then press the 'Reset' button."
        echo "Board will boot ino 'USB Mode'"
        echo "Exiting..."
        exit;
        ;;
esac

echo "Setting target to > $selected_board"
idf.py set-target "$selected_board"

# BUIL IS PART OF SET-BOARD
# Ask the user if they want to rebuild the code
# echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
# echo ""
# read -p "         Do you want to rebuild the code? This reset custom Menuconfig values (like SSID,..) (y/n, default: n): " choice
# case "$choice" in
#     [yY])
#         echo "Rebuilding the code..."
#         idf.py build
#        ;;
#     *)
#         echo "Ok."
#         ;;
# esac

# Ask the user if they want to rebuild the code
echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
read -p "         Do you want run Menuconfig? Set values like SSID,.. (y/n, default: n): " choice
case "$choice" in
    [yY])
        idf.py menuconfig
        ;;
    *)
        echo "Ok."
        ;;
esac

# Ask the user if they want to rebuild the code
echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
read -p "         Do you want start flashing Board via $PORT ? (y/n, default: n): " choice
case "$choice" in
    [yY])
        idf.py -p "$PORT" -b 115200 flash
        ;;
    *)
        echo "Ok."
        ;;
esac

echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
echo "         Waring: Serial monitor can be exited by CTRL + T, then X"
echo ""
echo "#### #### #### #### #### #### #### #### #### #### #### #### #### #### ####"
echo ""
read -p "         Do you want start Serial Monitor on $PORT ? (y/n, default: n): " choice
case "$choice" in
    [yY])
        idf.py -p "$PORT" -b 115200 monitor
        ;;
    *)
        echo "Ok. Exiting..."
        exit;
        ;;
esac
