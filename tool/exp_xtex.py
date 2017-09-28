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


def writeBits(bw, bits, nbits):
	nbytes = xd.ceilDiv(nbits, 8)
	wk = bits
	for i in xrange(nbytes):
		bw.writeU8(wk & 0xFF)
		wk >>= 8

class TexPlane:
	def __init__(self, xtex, name, rawFlg = not True):
		self.xtex = xtex
		self.name = name
		self.nameId = xtex.strLst.add(name)
		if name == "a":
			self.data = xtex.img.allPixels("A")
		else:
			self.data = xtex.img.allPixels("C", xh.getRGBComponentName(xtex.img, name))
		ref = self.data[0]
		self.constFlg = True
		for val in self.data:
			if val != ref:
				self.constFlg = False
				break
		self.compress(rawFlg)
		#print self.name, ":", len(self.data)*4, "v", ceilDiv(self.bitCnt, 8), self.valOffs, self.minTZ

	def compress(self, rawFlg):
		self.minVal = min(self.data)
		self.maxVal = max(self.data)
		self.valOffs = self.minVal
		if self.valOffs > 0: self.valOffs = 0
		self.bitCnt = 0
		self.bits = 0
		self.minTZ = 32
		if self.constFlg:
			self.format = 0
			return
		if rawFlg:
			self.format = -1
			return
		self.format = 1
		for fval in self.data:
			fval -= self.valOffs
			ival = xd.getBitsF32(fval) & ((1<<31)-1)
			self.minTZ = min(self.minTZ, xd.ctz32(ival))
		tblSize = 1 << 8
		tbl = [0 for i in xrange(tblSize)]
		pred = 0
		hash = 0
		nlenBits = 5
		w = self.xtex.w
		h = self.xtex.h
		for y in xrange(h):
			for x in xrange(w):
				idx = (h-1-y)*w + x
				fval = self.data[idx] - self.valOffs
				ival = xd.getBitsF32(fval) & ((1<<31)-1)
				ival >>= self.minTZ
				xor = ival ^ pred
				tbl[hash] = ival
				hash = ival >> 21
				hash &= tblSize - 1
				pred = tbl[hash]
				xlen = 0
				if xor: xlen = xd.bitLen32(xor)
				dat = xlen
				if xlen: dat |= (xor & ((1<<xlen)-1)) << nlenBits
				self.bits |= dat << self.bitCnt
				self.bitCnt += nlenBits + xlen

	def writeInfo(self, bw):
		bw.writeU32(0) # +00 -> data
		self.xtex.writeStrId16(bw, self.nameId) # +04
		bw.writeU8(self.minTZ) # +06
		bw.writeI8(self.format) # +07 
		bw.writeF32(self.minVal) # +08
		bw.writeF32(self.maxVal) # +0C
		bw.writeF32(self.valOffs) # +10
		bw.writeU32(self.bitCnt) # +14
		bw.writeU32(0) # +18 reserved0
		bw.writeU32(0) # +1C reserved1

	def writeData(self, bw):
		if self.format == 0:
			bw.writeF32(self.data[0])
		elif self.format == 1:
			writeBits(bw, self.bits, self.bitCnt)
		else:
			w = self.xtex.w
			h = self.xtex.h
			for y in xrange(h):
				for x in xrange(w):
					idx = (h-1-y)*w + x
					bw.writeF32(self.data[idx])

class TexExporter(xd.BaseExporter):
	def __init__(self):
		xd.BaseExporter.__init__(self)
		self.sig = "XTEX"

	def build(self, copPath, rawFlg = True):
		self.copPath = copPath
		self.nameId, self.pathId = self.strLst.addNameAndPath(copPath)
		self.img = hou.node(copPath)
		self.w = self.img.xRes()
		self.h = self.img.yRes()
		self.planes = {}
		self.addPlane("r", rawFlg)
		self.addPlane("g", rawFlg)
		self.addPlane("b", rawFlg)
		self.addPlane("a", rawFlg)

	def addPlane(self, name, rawFlg = True):
		self.planes[name] = TexPlane(self, name, rawFlg)

	def writeHead(self, bw, top):
		npln = len(self.planes)
		bw.writeU32(self.w) # +20
		bw.writeU32(self.h) # +24
		bw.writeU32(npln) # +28
		self.patchPos = bw.getPos()
		bw.writeI32(0) # +2C -> info

	def writeData(self, bw, top):
		plnLst = []
		for plnName in self.planes: plnLst.append(self.planes[plnName])
		npln = len(plnLst)
		bw.align(0x10)
		infoTop = bw.getPos()
		bw.patch(self.patchPos, bw.getPos() - top) # -> info
		for i in xrange(npln):
			plnLst[i].writeInfo(bw)
		for i, pln in enumerate(plnLst):
			bw.align(4)
			bw.patch(infoTop + (i*0x20), bw.getPos() - top)
			print "Saving plane", pln.name
			pln.writeData(bw)

	def save(self, outPath):
		xd.BaseExporter.save(self, outPath)

if __name__=="__main__":
	outPath = hou.expandString("$HIP/")
	outPath = exePath
	#outPath = r"D:/tmp/"
	outPath += r"\_test.xtex"
	
	#copPath = "/obj/IBL_TEX/OUT"
	copPath = "/obj/TEST_TEX/OUT"
	xtex = TexExporter()
	xtex.build(copPath)

	xtex.save(outPath)
