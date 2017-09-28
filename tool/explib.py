import sys
import hou
import os
import imp
import re
import inspect

exePath = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename))

libName = "xd"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
import xd

libName = "xh"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
import xh

libName = "exp_xval"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
from exp_xval import *

libName = "exp_xtex"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
from exp_xtex import *

libName = "exp_xgeo"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
from exp_xgeo import *

libName = "exp_xrig"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
from exp_xrig import *

libName = "exp_xkfr"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
from exp_xkfr import *

libName = "exp_xcel"
libFile, libFname, libDescr = imp.find_module(libName, [exePath])
imp.load_module(libName, libFile, libFname, libDescr)
from exp_xcel import *


class FileCatEntry:
	def __init__(self, nameId, fnameId):
		self.nameId = nameId
		self.fnameId = fnameId

class FileCatalogue(xd.BaseExporter):
	def __init__(self):
		xd.BaseExporter.__init__(self)
		self.sig = "FCAT"
		self.lst = []

	def addFile(self, name, fname):
		nameId  = self.strLst.add(name)
		fnameId = self.strLst.add(fname)
		self.lst.append(FileCatEntry(nameId, fnameId))

	def writeHead(self, bw, top):
		n = len(self.lst)
		bw.writeU32(n) # +20
		self.patchPos = bw.getPos()
		bw.writeU32(0) # -> entries[]

	def writeData(self, bw, top):
		bw.align(0x10)
		bw.patch(self.patchPos, bw.getPos() - top)
		for ent in self.lst:
			self.writeStrId32(bw, ent.nameId)
			self.writeStrId32(bw, ent.fnameId)


def exportXTEX(copPath, outPath, useCmd = True, rawFlg = True):
	sres = ("", "")
	if useCmd: sres = hou.hscript("xtex -o"+outPath+" "+copPath)
	if not useCmd or sres[1].startswith("Unknown command"):
		if useCmd: print "Warning: falling back to ref. XTEX exporter."
		xtex = TexExporter()
		xtex.build(copPath, rawFlg)
		xtex.save(outPath)

def exportXGEO(sopPath, outPath, bvhFlg = False, skinSphFlg = False):
	sop = hou.node(sopPath)
	if not sop: return
	xgeo = GeoExporter()
	xgeo.build(sop, bvhFlg = bvhFlg, skinSphFlg = skinSphFlg)
	xgeo.save(outPath)

def exportStage(sop = None, sopOBS = None, sopLFD = None, outPath = None, flgMDL = True, flgTEX = True, flgMTL = True, flgOBS = True, flgLFD = True, verbose = True):
	if not sop:
		sop = xh.findSOPEXP()
	if not sop: return
	if not outPath:
		outPath = hou.expandString("$HIP/xou_exp/")
	if not os.path.exists(outPath): os.makedirs(outPath)
	if verbose:
		print "Stage geo path:", sop.path()
		print "Stage out path:", outPath
	outName = sop.parent().name()
	outBase = outPath + outName
	if flgMDL:
		outPathMDL = outBase + ".xmdl"
		if verbose:
			print "Exporting stage model geometry to", outPathMDL
		xgeo = GeoExporter()
		xgeo.build(sop)
		xgeo.save(outPathMDL)
	if flgMTL:
		outPathMTL = outBase + ".xmtl"
		print "Exporting stage materials to", outPathMTL
		mtls = xh.getSOPMtlList(sop)
		shopLst = []
		for info in mtls:
			if info.mtl:
				shopLst.append(info.mtl)
		expVal(shopLst, outPathMTL)
	if flgTEX:
		baseMaps = xh.getSOPBaseMapList(sop)
		for tpath in baseMaps:
			if tpath.startswith("op:"):
				print "Exporting stage texture:", tpath
				tname = hou.node(tpath).name()
				outPathTEX = outPath + tname + ".xtex"
				print outPathTEX
				exportXTEX(tpath, outPathTEX)
	if flgOBS:
		if not sopOBS:
			sopOBS = xh.findSOPEXP(expNodeName = "EXP_OBS")
		if not sopOBS: return
		if verbose:
			print "Stage obstacle SOP path:", sopOBS.path()
		outPathOBS = outBase + ".xobs"
		if verbose:
			print "Exporting stage collision geometry to", outPathOBS
		xgeo = GeoExporter()
		xgeo.build(sopOBS, bvhFlg = True)
		xgeo.save(outPathOBS)

	if flgLFD:
		if not sopLFD:
			sopLFD = xh.findSOPEXP(expNodeName = "EXP_LFD")
		if not sopLFD: return
		if verbose:
			print "Stage lightfield SOP path:", sopLFD.path()
		outPathLFD = outBase + ".xlfd"
		if verbose:
			print "Exporting stage lightfield geometry to", outPathLFD
		xgeo = GeoExporter()
		xgeo.build(sopLFD, bvhFlg = True)
		xgeo.save(outPathLFD)

def exportProp():
	pass # TODO

def exportActor(sop = None, rigRootPath = None, outPath = None, flgMDL = True, flgRIG = True, flgTEX = True, flgMTL = True, rawTex = True, verbose = True):
	if not sop:
		sop = xh.findSOPEXP()
		if not sop: sop = xh.findSkinNode()
	if not sop: return
	if not outPath:
		outPath = hou.expandString("$HIP/xou_exp/")
	if not os.path.exists(outPath): os.makedirs(outPath)
	if verbose:
		print "Actor geo path:", sop.path()
		print "Actor out path:", outPath
	outName = sop.parent().name()
	outBase = outPath + outName
	if flgMDL:
		outPathMDL = outBase + ".xmdl"
		if verbose:
			print "Exporting actor geometry to", outPathMDL
		xgeo = GeoExporter()
		xgeo.build(sop)
		xgeo.save(outPathMDL)
	if flgRIG:
		if not rigRootPath:
			rigRootPath = "/obj/root"
		outPathRIG = outBase + ".xrig"
		if verbose:
			print "Rig root path:", rigRootPath
			print "Exporting rig data to", outPathRIG
		rig = RigExporter()
		rig.build(rigRootPath)
		rig.save(outPathRIG)

		cel = getExprLibFromHrc(rigRootPath)
		if cel:
			outPathCEL = outBase + ".xcel"
			print "Exporting expressions library to", outPathCEL
			cel.save(outPathCEL)
	if flgMTL:
		outPathMTL = outBase + ".xmtl"
		print "Exporting actor materials to", outPathMTL
		mtls = xh.getSOPMtlList(sop)
		shopLst = []
		for info in mtls:
			if info.mtl:
				shopLst.append(info.mtl)
		expVal(shopLst, outPathMTL)
	if flgTEX:
		baseMaps = xh.getSOPBaseMapList(sop)
		for tpath in baseMaps:
			if tpath.startswith("op:"):
				print "Exporting texture:", tpath
				tname = hou.node(tpath).name()
				#outPathTEX = outBase + "." + tname + ".xtex"
				outPathTEX = outPath + tname + ".xtex"
				print outPathTEX
				exportXTEX(tpath, outPathTEX)
