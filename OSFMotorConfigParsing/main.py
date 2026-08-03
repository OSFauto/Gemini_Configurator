if __name__ == '__main__':
    filenames = ["BE232DJ", "BE343JJ", "MPP100-3D1E", "MPP115-3C1E"]
    with open("output.txt", "w", encoding="utf-8") as writefile:
        for filename in filenames:
            commands = []
            print(filename)
            with open(filename+".txt", "r", encoding="utf-8") as file:
                for line in file:
                    if line.startswith(";") or line.strip() == "":
                        continue
                    clean_line = line.split(";")[0].strip()
                    command = clean_line.split()
                    commands.append(command)

            writefile.write("inline Command "+filename.replace("-","_")+"_Config[] = \n{\n")
            for command in commands:
                writefile.write("\t{\""+command[0] + "\",\""+command[1]+"\"},\n")
            writefile.write("};\n")
            writefile.write("inline int "+filename.replace("-","_")+"_Config_length = "+str(len(commands))+";\n\n")


