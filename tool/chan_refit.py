import sys
import hou
import os
import imp
import re
import inspect
from math import *
from array import array

exePath = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename))

libName = "xh"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
import xh

def refit(chLst, rotTol = 0.25, posTol = 0.0001, excludeNodes = None):
	start = xh.getMinFrame()
	end = xh.getMaxFrame()
	print "Refit range:", start, "to", end
	for chPath in chLst:
		prm = hou.parm(chPath)
		tol = posTol
		if prm.name() in ["rx", "ry", "rz"]: tol = rotTol
		flg = True
		if excludeNodes:
			if prm.node().name() in excludeNodes:
				flg = False
		if flg:
			print "Refitting", chPath
			prm.keyframesRefit(True, tol, True, False, False, 0, 0, True, start, end, hou.parmBakeChop.Off)
		else:
			print "Skipping", chPath

if __name__=="__main__":
	refit(xh.getChannelsInGroup("MOT"), excludeNodes = ["n_Move"])
