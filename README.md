# Gemini Configurator

To Use:
- Download "libraries" and "OSFDriveCabinetGiga" to the same directory (note: not all libraries in the libraries folder are used)
- Open "OSFDriveCabinetGiga.ino" using the arduino ide and upload to the giga
- Wire serial rx and tx to pins 18 and 19 on the Giga (Tx1 and Rx1)
- The OSFDriveCabinetUI folder contains the GUI project for future editing and adjustments using Squareline Studio

To Update Config values:
- Download OSFMotorConfigParsing
- Ensure Python is installed
- Update the files as needed
    - To add a new config file, create the text file and add it to the filenames array
- Run the main script using "py main.py" in the command line within the OSFMotorConfigParsing directory
- Copy the output.txt text into configs.h in the OSFDriveCabinetGiga directory **Do not overwrite the entire file, paste the values after the end of the Command struct definition**
- If a new config file has been added, "OSFDriveCabinetGiga.ino" will need to be updated accordingly. 
