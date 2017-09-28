# Author: Sergey Chaban <sergey.chaban@gmail.com>

import sys
import hou
import os
import imp
import re
import inspect
from math import *
from array import array

exePath = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename))

libName = "xd"
try: sys.modules[libName]
except:
	libFile, libFname, libDescr = imp.find_module(libName, [exePath])
	imp.load_module(libName, libFile, libFname, libDescr)
import xd

libName = "xh"
try: sys.modules[libName]
except:
	libFile, libFname, libDescr = imp.find_module(libName, [exePath])
	imp.load_module(libName, libFile, libFname, libDescr)
import xh


class ExprEntry:
	def __init__(self, exprText, nodeName, nodePath = None, chanName = None):
		self.nodeName = nodeName
		self.nodeNameId = -1
		self.nodePath = nodePath
		self.nodePathId = -1
		self.chanName = chanName
		self.chanNameId = -1
		self.exprText = exprText
		e = xd.Expr()
		e.compile(exprText)
		self.compiled = e

	def addToLib(self, lib):
		self.lib = lib
		if not lib: return
		strLst = lib.strLst
		if self.nodeName: self.nodeNameId = strLst.add(self.nodeName)
		if self.nodePath: self.nodePathId = strLst.add(self.nodePath)
		if self.chanName: self.chanNameId = strLst.add(self.chanName)

	def printInfo(self):
		print self.exprText,  "@", self.nodeName, self.nodePath, self.chanName

	def write(self, bw):
		lib = self.lib
		if not lib: return
		lib.writeStrId16(bw, self.nodeNameId)
		lib.writeStrId16(bw, self.nodePathId)
		lib.writeStrId16(bw, self.chanNameId)
		bw.writeI16(0) # reserved
		self.compiled.write(bw)

class ExprLibExporter(xd.BaseExporter):
	def __init__(self):
		xd.BaseExporter.__init__(self)
		self.sig = "XCEL"
		self.exprLst = []

	def setLst(self, lst):
		for entry in lst:
			entry.addToLib(self)
			self.exprLst.append(entry)

	def writeHead(self, bw, top):
		nexp = len(self.exprLst)
		bw.writeU32(nexp) # +20
		self.patchPos = bw.getPos()
		bw.writeU32(0) # -> expr[]

	def writeData(self, bw, top):
		bw.align(0x10)
		bw.patch(self.patchPos, bw.getPos() - top)
		nexp = len(self.exprLst)
		patchTop = bw.getPos()
		for i in xrange(nexp):
			bw.writeU32(0)
		for i, entry in enumerate(self.exprLst):
			bw.align(0x10)
			bw.patch(patchTop + i*4, bw.getPos() - top)
			entry.write(bw)
			if 0:
				print ">>>> " + entry.nodeName + ":" + entry.chanName
				entry.compiled.disasm()

def getExprLibFromHrc(rootPath="/obj/root"):
	lib = None
	hrc = xh.NodeList()
	hrc.hrcBuild(rootPath)
	excl = ["cubic()", "linear()", "qlinear()", "constant()", "bezier()"]
	lst = []
	for node in hrc.nodes:
		for parm in node.parms():
			try: expr = parm.expression()
			except: expr = None
			if expr and not expr in excl:
				nodeName = node.name()
				nodePath = None
				fullPath = node.path()
				sep = fullPath.rfind("/")
				if sep >= 0: nodePath = fullPath[:sep]
				chanName = parm.name()
				ent = ExprEntry(expr, nodeName, nodePath, chanName)
				#ent.printInfo()
				lst.append(ent)
	if len(lst) > 0:
		lib = ExprLibExporter()
		lib.setLst(lst)
	return lib

def expExprLibFromHrc(outPath, rootPath="/obj/root"):
	lib = getExprLibFromHrc(rootPath)
	lib.save(outPath)

if __name__=="__main__":
	outPath = hou.expandString("$HIP/")
	outPath = exePath
	#outPath = r"D:/tmp/"
	outPath += "/_test.xcel"

	expExprLibFromHrc(outPath, rootPath="/obj/ANIM/root")

	"""
	e = xd.Expr()
	etext = 'ch("../j_Wrist_R/rx") * clamp(abs(sin(ch("../j_Wrist_R/rx"))) + 0.1, 0.0, 1.0)'
	e.compile(etext)
	e.disasm()
	"""

	"""
	#ExprEntry('frac(-2.7)', "test") #
	ee0 = ExprEntry('ch("../j_Wrist_R/rx") * abs(sin(ch("../j_Wrist_R/rx")))', "j_Wrist_R", "/obj", "rx") #ExprEntry('sin(12.3 + 45.6)', "test")
	ee1 = ExprEntry('sin((($FF % 30) / 30) * 180) * sin(pow(abs(((($FF+15) % 15) / 15)*2-1), 1.5)*90)', "bounce")
	ee2 = ExprEntry('0.0774506 - sqrt(-clamp(sin((($FF % 60) / 60) * 180)/80 - 0.01, -0.05, 0) * int($FF%90/60))/5', "blink")
	ee3 = ExprEntry('ch("../j_Wrist_R/rx") * clamp(abs(sin(ch("../j_Wrist_R/rx")) + 0.1), 0.0, 1.0)', "j_Wrist_R", "/obj", "rx")
	ee4 = ExprEntry('ch("../j_Wrist_L/rx") * clamp(abs(sin(ch("../j_Wrist_L/rx")) + 0.1), 0.0, 1.0)', "j_Wrist_L", "/obj", "rx")

	lib = ExprLibExporter()
	lib.setLst([ee0, ee1, ee2, ee3, ee4])
	lib.save(outPath)
	"""
