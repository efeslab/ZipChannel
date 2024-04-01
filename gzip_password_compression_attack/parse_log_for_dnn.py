#!/bin/python
import sys
import csv
csv.field_size_limit(sys.maxsize)
import numpy as np
import pandas as pd

def ReadTextLines(FullFilePath):
    """Read the string content of a text file given by FullFilePath and return it as a string."""
    with open(FullFilePath, "r+", encoding = "utf8") as io:
        TextString = io.readlines()
    return TextString


DependentVarName    = "Target"
ColumnsToKeepY      = [DependentVarName]

#############################
### Reading From One File ###
XY_DF = ReadTextLines(f"dnn_log.txt")
XY_DF = [XY_DF[i][1:-2].replace("'", "") for i in range(len(XY_DF))]

reader = csv.reader(XY_DF, delimiter=',')
Class = []
Feature1 = []
Feature2 = []
for row in reader:
    Class.append(row[0])
    Feature1.append(row[1].strip().split(" "))
    Feature2.append(row[2].strip().split(" "))
Class = np.array(Class)
Feature1 = np.array(Feature1)
Feature2 = np.array(Feature2)

XY_DF = pd.concat([
    pd.DataFrame(Class, columns = ColumnsToKeepY),
    pd.DataFrame(Feature1, columns = [f"{i + 1}" for i in range(Feature1.shape[1])]).astype(np.float32),
    pd.DataFrame(Feature2, columns = [f"{i + 10001}" for i in range(Feature2.shape[1])]).astype(np.float32)
], axis = 1)


XY_DF.to_csv('concatenated_data.csv', index=False)
