# Author: Sergey Chaban <sergey.chaban@gmail.com>

import sys
import os
import struct
import inspect
from math import *
from array import array

try: xrange
except: xrange = range

def dbgmsg(msg):
	sys.stdout.write(str(msg) + "\n")

def defNodeName(id):
	return 'nd_%(#)02d' % {"#":id}

def vecPad(v, n, pad = 0.0):
	l = len(v)
	if l < n: v = list(v) + [pad for i in xrange(n-l)]
	return v

def cvtLinClrChan(val):
	if val < 0: return val;
	return pow(val, 1.0/2.2);

class StrList:
	def __init__(self):
		self.clear()

	def clear(self):
		self.lst = []
		self.map = {}
		self.cnt = 0

	def add(self, str):
		if str not in self.map:
			self.map[str] = len(self.lst)
			self.lst.append(str)
			self.cnt += len(str) + 1
		return self.map[str]

	def get(self, idx):
		if idx < 0 or idx >= len(self.lst): return None
		return self.lst[idx]

	def getIdx(self, str):
		if str in self.map: return self.map[str]
		return -1

	def read(self, f):
		top = f.tell()
		(dsize, nstr) = struct.unpack("<II", f.read(8))
		if dsize and nstr:
			offsLst = struct.unpack("<" + str(nstr) + "I", f.read(4*nstr))
		for offs in offsLst:
			f.seek(top + offs)
			data = array("B")
			i = 0
			while True:
				data.fromfile(f, 1)
				if not data[i]:
					data.pop()
					break
				i += 1
			s = data.tostring().decode()
			self.add(s)

class Poly:
	def __init__(self, xgeo):
		self.xgeo = xgeo
		self.vtxNum = 0
		self.mtlId = 0

	def readVtxList(self, nvtx, f):
		if not self.xgeo: return
		self.vtxNum = nvtx
		self.vtxLst = []
		vsize = self.xgeo.getVtxIdxSize()
		if vsize > 0:
			for i in xrange(nvtx):
				idx = -1
				if vsize == 1:
					(idx,) = struct.unpack("<B", f.read(1))
				elif vsize == 2:
					(idx,) = struct.unpack("<H", f.read(2))
				elif vsize == 3:
					(l, h) = struct.unpack("<HB", f.read(3))
					idx = l | (h<<16)
				self.vtxLst.append(idx)

class AttrInfo:
	def __init__(self, xgeo):
		self.xgeo = xgeo

	def read(self, f):
		if not self.xgeo: return
		(self.dataOffs, self.nameId, self.elemNum, self.elemType, reserved) = struct.unpack("<IiHBB", f.read(12))
		self.name = self.xgeo.strLst.get(self.nameId)

	def isInt(self): return self.elemType == 0
	def isFloat(self): return self.elemType == 1
	def isString(self): return self.elemType == 3

class AttrClass:
	def __init__(self): pass
AttrClass.GLB = 0
AttrClass.PNT = 1
AttrClass.POL = 2

class Attr:
	def __init__(self, info, cls):
		self.info = info
		self.cls = cls

	def getName(self): return self.info.name

	def read(self, f):
		info = self.info
		offs = info.dataOffs
		n = 1
		if self.cls == AttrClass.PNT:
			n = info.xgeo.pntNum
		elif self.cls == AttrClass.POL:
			n = info.xgeo.polNum
		f.seek(offs)
		self.data = []
		fmt = None
		size = 0
		if info.isInt():
			fmt = "<" + str(info.elemNum) + "i"
			size = info.elemNum*4
		if info.isFloat():
			fmt = "<" + str(info.elemNum) + "f"
			size = info.elemNum*4
		if fmt and size:
			for i in xrange(n):
				rec = list(struct.unpack(fmt, f.read(size)))
				self.data.append(rec)

class GrpInfo:
	def __init__(self, xgeo):
		self.xgeo = xgeo

	def read(self, f):
		if not self.xgeo: return
		(self.nameId, self.pathId) = struct.unpack("<hh", f.read(4))
		(minx, miny, minz, maxx, maxy, maxz) = struct.unpack("<6f", f.read(6*4))
		self.bboxMin = [minx, miny, minz]
		self.bboxMax = [maxx, maxy, maxz]
		(self.minIdx, self.maxIdx) = struct.unpack("<ii", f.read(8))
		(self.maxWgtNum, self.skinNodeNum, self.idxNum) = struct.unpack("<HHI", f.read(8))
		self.name = self.xgeo.strLst.get(self.nameId)
		self.path = self.xgeo.strLst.get(self.pathId)
		self.dataOffs = f.tell()

	def getIdxSize(self):
		idxSpan = self.maxIdx - self.minIdx
		res = 0
		if idxSpan < (1<<8):
			res = 1
		elif idxSpan < (1<<16):
			res = 2
		else:
			res = 3
		return res
		

class Group:
	def __init__(self, info):
		self.info = info

	def read(self, f):
		info = self.info
		offs = info.dataOffs
		nskn = info.skinNodeNum
		if nskn > 0:
			if info.xgeo.hasSkinSpheres():
				offs += 0x10*nskn
			offs += nskn*2
		idxSize = info.getIdxSize()
		f.seek(offs)
		nidx = info.idxNum
		self.idx = []
		for i in xrange(nidx):
			idx = 0
			if idxSize == 1:
				(idx,) = struct.unpack("<B", f.read(1))
			elif idxSize == 2:
				(idx,) = struct.unpack("<H", f.read(2))
			elif idxSize == 3:
				(l, h) = struct.unpack("<HB", f.read(3))
				idx = l | (h<<16)
			idx += info.minIdx
			self.idx.append(idx)

class XGeo:
	def __init__(self):
		self.flags = 0
		self.fileSize = 0
		self.strLst = StrList()

	def isLoaded(self): return self.fileSize > 0

	def isSamePolSize(self): return bool(self.flags & 1)

	def isSamePolMtl(self): return bool(self.flags & 2)

	def hasSkinSpheres(self): return bool(self.flags & 4)

	def isAllTris(self):
		if self.isLoaded():
			return self.isSamePolSize() and self.maxVtxPerPol == 3
		return False

	def getVtxIdxSize(self):
		size = 0
		if self.isLoaded():
			if self.pntNum <= (1<<8):
				size = 1
			elif self.pntNum <= (1<<16):
				size = 2
			else:
				size = 3
		return size

	def getMtlIdSize(self):
		size = 0
		if self.isLoaded() and not self.isSamePolMtl():
			if self.mtlNum < (1<<7): size = 1
			else: size = 2
		return size

	def readHead(self, f):
		kindStr = "XGEO"
		# common
		head = f.read(0x20)
		(kind, flg, fsize, hsize, soffs, nameId, pathId) = struct.unpack("<4sIIIIhh", head[:0x18])
		kind = kind.decode()
		if kind != kindStr:
			dbgmsg("!" + kindStr)
			return
		self.flags = flg
		self.fileSize = fsize
		self.headSize = hsize
		self.strOffs = soffs
		self.nameId = nameId
		self.pathId = pathId
		# geo
		head = f.read(0x40)
		(minx, miny, minz, maxx, maxy, maxz) = struct.unpack("<6f", head[:0x18])
		self.bboxMin = [minx, miny, minz]
		self.bboxMax = [maxx, maxy, maxz]
		# counts
		(npnt, npol, nmtl, nglbAttr, npntAttr, npolAttr, npntGrp, npolGrp, nskin, maxSkinWgt, maxPolVtx) = struct.unpack("<9IHH", head[0x18:])
		self.pntNum = npnt
		self.polNum = npol
		self.mtlNum = nmtl
		self.glbAttrNum = nglbAttr
		self.pntAttrNum = npntAttr
		self.polAttrNum = npolAttr
		self.pntGrpNum = npntGrp
		self.polGrpNum = npolGrp
		self.skinNodeNum = nskin
		self.maxSkinWgtNum = maxSkinWgt
		self.maxVtxPerPol = maxPolVtx
		# offsets
		noffs = 11
		offs = f.read(noffs*4)
		(offsPnt, offsPol, offsMtl, offsGlbAttr, offsPntAttr, offsPolAttr, offsPntGrp, offsPolGrp, offsSkinNodes, offsSkin, offsBVH) = struct.unpack("<"+str(noffs)+"I", offs)
		self.pntOffs = offsPnt
		self.polOffs = offsPol
		self.mtlOffs = offsMtl
		self.glbAttrOffs = offsGlbAttr
		self.pntAttrOffs = offsPntAttr
		self.polAttrOffs = offsPolAttr
		self.pntGrpOffs = offsPntGrp
		self.polGrpOffs = offsPolGrp
		self.skinNodesOffs = offsSkinNodes
		self.skinOffs = offsSkin

	def readStrs(self, f):
		if self.strOffs <= 0: return
		f.seek(self.strOffs)
		self.strLst.read(f)

	def readPnts(self, f):
		if self.pntOffs <= 0: return
		#print hex(self.pntNum), "pts @", hex(self.pntOffs)
		self.pnts = []
		f.seek(self.pntOffs)
		for i in xrange(self.pntNum):
			vdata = f.read(4*3)
			(x, y, z) = struct.unpack("<3f", vdata)
			self.pnts.append([x, y, z])

	def readPols(self, f):
		if self.polOffs <= 0: return
		self.pols = []
		npol = self.polNum
		for i in xrange(npol):
			self.pols.append(Poly(self))
		if not self.isSamePolMtl():
			top = self.polOffs
			if not self.isSamePolSize():
				top += npol * 4
			size = self.getMtlIdSize()
			if size > 0:
				fmt = "<" + ["b", "h"][size-1]
				f.seek(top)
				for i in xrange(npol):
					(mtlId,) = struct.unpack(fmt, f.read(size))
					self.pols[i].mtlId = mtlId
		size = self.getVtxIdxSize()
		if size > 0:
			for i in xrange(npol):
				top = 0
				nvtx = 0
				if self.isSamePolSize():
					nvtx = self.maxVtxPerPol
					top = self.polOffs
					if not self.isSamePolMtl():
						top += npol * self.getMtlIdSize()
					vlstSize = self.getVtxIdxSize() * self.maxVtxPerPol
					top += vlstSize*i
				else:
					f.seek(self.polOffs + i*4)
					(top,) = struct.unpack("<I", f.read(4))
					ntop = self.polOffs + npol*4
					if not self.isSamePolMtl():
						ntop += npol * self.getMtlIdSize()
					if self.maxVtxPerPol < (1<<8):
						f.seek(ntop + i)
						(nvtx,) = struct.unpack("<B", f.read(1))
					else:
						f.seek(ntop + i*2)
						(nvtx,) = struct.unpack("<H", f.read(2))
				if top > 0:
					f.seek(top)
					self.pols[i].readVtxList(nvtx, f)

	def readPntAttrs(self, f):
		if self.pntAttrOffs <= 0: return
		f.seek(self.pntAttrOffs)
		self.pntAttrInfos = []
		self.pntAttrs = []
		for i in xrange(self.pntAttrNum):
			info = AttrInfo(self)
			info.read(f)
			self.pntAttrs.append(Attr(info, AttrClass.PNT))
		self.pntAttrMap = {}
		for i in xrange(self.pntAttrNum):
			self.pntAttrs[i].read(f)
			self.pntAttrMap[self.pntAttrs[i].getName()] = i

	def readSkin(self, f):
		if self.skinOffs <= 0: return
		npnt = self.pntNum
		f.seek(self.skinOffs)
		offsLst = struct.unpack("<" + str(npnt) + "I", f.read(npnt*4))
		nwgtLst = struct.unpack("<" + str(npnt) + "B", f.read(npnt))
		nnode = self.skinNodeNum
		if nnode <= (1<<8):
			jidxFmt = "B"
			jidxSize = 1
		else:
			jidxFmt = "H"
			jidxSize = 2
		self.pntWgts = []
		self.pntJnts = []
		for i in xrange(npnt):
			f.seek(offsLst[i])
			nwgt = nwgtLst[i]
			pntWgt = struct.unpack("<" + str(nwgt) + "f", f.read(nwgt*4))
			pntJnt = struct.unpack("<" + str(nwgt) + jidxFmt, f.read(nwgt*jidxSize))
			self.pntWgts.append(pntWgt)
			self.pntJnts.append(pntJnt)
		self.nodeNames = []
		if self.skinNodesOffs > 0:
			f.seek(self.skinNodesOffs + nnode*0x10) # skip bounding spheres
			for i in xrange(nnode):
				(nameId,) = struct.unpack("<i", f.read(4))
				nodeName = self.strLst.get(nameId)
				if not nodeName: nodeName = defNodeName(i)
				self.nodeNames.append(nodeName)
		else:
			for i in xrange(nnode):
				self.nodeNames.append(defNodeName(i))

	def readMtlGrps(self, f):
		if self.mtlOffs <= 0: return
		nmtl = self.mtlNum
		self.mtlGrps = []
		for i in xrange(nmtl):
			f.seek(self.mtlOffs + i*4)
			(offs,) = struct.unpack("<I", f.read(4))
			f.seek(offs)
			info = GrpInfo(self)
			info.read(f)
			self.mtlGrps.append(Group(info))
		for i in xrange(nmtl):
			self.mtlGrps[i].read(f)

	def read(self, f):
		self.readHead(f)
		if not self.isLoaded(): return
		self.readStrs(f)
		self.readPnts(f)
		self.readPols(f)
		self.readPntAttrs(f)
		self.readMtlGrps(f)
		self.readSkin(f)

	def load(self, fpath):
		f = open(fpath, "rb")
		if not f: return
		self.read(f)
		f.close()

	def getName(self):
		name = "<unknown>"
		if self.isLoaded() and self.nameId >= 0:
			name = self.strLst.get(self.nameId)
		return name

	def getPath(self):
		path = ""
		if self.isLoaded() and self.pathId >= 0:
			path = self.strLst.get(self.pathId)
		return path

	def findPntAttrIdx(self, name):
		idx = -1
		if self.isLoaded():
			if name in self.pntAttrMap:
				idx = self.pntAttrMap[name]
		return idx

	def ckPntIdx(self, idx):
		if not self.isLoaded(): return False
		return idx >= 0 and idx < self.pntNum

	def getPntColor(self, idx):
		clr = [1.0, 1.0, 1.0]
		if self.isLoaded() and self.ckPntIdx(idx):
			attrIdx = self.findPntAttrIdx("Cd")
			if attrIdx >= 0:
				attr = self.pntAttrs[attrIdx]
				clr = attr.data[idx]
		return clr

	def getPntNormal(self, idx):
		nrm = [0.0, 1.0, 0.0]
		if self.isLoaded() and self.ckPntIdx(idx):
			attrIdx = self.findPntAttrIdx("N")
			if attrIdx >= 0:
				attr = self.pntAttrs[attrIdx]
				nrm = attr.data[idx]
		return nrm

	def getPntTex(self, idx):
		tex = [0.0, 0.0, 1.0]
		if self.isLoaded() and self.ckPntIdx(idx):
			attrIdx = self.findPntAttrIdx("uv")
			if attrIdx >= 0:
				attr = self.pntAttrs[attrIdx]
				tex = vecPad(attr.data[idx], 3, 1.0)
		return tex

	def saveLS(self, outPath, useMtls = True, useSkin = True):
		f = open(outPath, "w")
		if not f: return
		npnt = self.pntNum
		npol = self.polNum
		nmtl = self.mtlNum
		mtlFlg = useMtls and (nmtl > 0)
		f.write('// ' + str(sys.version_info) + '\n')
		f.write('main {\n')
		f.write('editbegin();\n')
		clrFlg = self.findPntAttrIdx("Cd") >= 0
		if clrFlg: f.write('var cmap = VMap(VMRGB, "Cd", 3);\n')
		nrmFlg = self.findPntAttrIdx("N") >= 0
		if nrmFlg: f.write("var nmap = VMap(@'N','O','R','M'@, "+'"Norm", 3);\n')
		texFlg = self.findPntAttrIdx("uv") >= 0
		if texFlg: f.write('var tmap = VMap(VMTEXTURE, "uv", 2);\n')
		if npnt > 0: f.write('var pid['+str(npnt)+'];\n');
		if texFlg: f.write('var uv[2];\n');
		sknFlg = useSkin and self.skinOffs > 0
		if sknFlg:
			f.write('var wmaps[' + str(self.skinNodeNum) + '];\n');
			for i in xrange(self.skinNodeNum):
				f.write('wmaps[' + str(i+1) + '] = VMap(VMWEIGHT, "' + self.nodeNames[i] + '", 1);\n')
		for i in xrange(npnt):
			pos = self.pnts[i]
			clr = self.getPntColor(i)
			clr = [cvtLinClrChan(clr[j]) for j in xrange(3)] #
			pos[2] = -pos[2] # RH->LH
			f.write('pid['+str(i+1)+']=addpoint(<'+str(pos[0])+','+str(pos[1])+','+str(pos[2])+'>);\n');
			if clrFlg:
				f.write('cmap.setValue(pid['+str(i+1)+'], <'+str(clr[0])+','+str(clr[1])+','+str(clr[2])+'>);\n');
			if nrmFlg:
				nrm = self.getPntNormal(i)
				nrm[2] = -nrm[2] # RH->LH
				f.write('nmap.setValue(pid['+str(i+1)+'], <'+str(nrm[0])+','+str(nrm[1])+','+str(nrm[2])+'>);\n');
			if texFlg:
				tex = self.getPntTex(i)
				f.write('uv[1]='+str(tex[0])+';\n');
				f.write('uv[2]='+str(tex[1])+';\n');
				f.write('tmap.setValue(pid['+str(i+1)+'], uv);\n');
			if sknFlg:
				pjnt = self.pntJnts[i]
				pwgt = self.pntWgts[i]
				nwgt = len(pjnt)
				for j in xrange(nwgt):
					f.write('wmaps[' + str(pjnt[j]+1) + '].setValue(pid['+str(i+1)+'], ' + str(pwgt[j]) + ');\n')
		if mtlFlg:
			f.write('editend();\n')
			for i in xrange(nmtl):
				mtlGrp = self.mtlGrps[i]
				mname = mtlGrp.info.name
				f.write('setsurface("' + mname + '");\n')
				f.write('editbegin();\n')
				for polId in mtlGrp.idx:
					nvtx = self.pols[polId].vtxNum
					vlst = self.pols[polId].vtxLst
					f.write('{\n')
					f.write('var vtx['+str(nvtx)+'];\n')
					for j in xrange(nvtx):
						f.write('vtx['+str(j+1)+']=pid['+str(vlst[j]+1)+'];\n')
					f.write('addpolygon(vtx);\n')
					f.write('}\n')
				f.write('editend();\n')
		else:
			for i in xrange(npol):
				nvtx = self.pols[i].vtxNum
				vlst = self.pols[i].vtxLst
				f.write('{\n')
				f.write('var vtx['+str(nvtx)+'];\n')
				for j in xrange(nvtx):
					f.write('vtx['+str(j+1)+']=pid['+str(vlst[j]+1)+'];\n')
				f.write('addpolygon(vtx);\n')
				f.write('}\n')
			f.write('editend();\n')
		f.write('}\n')
		f.close()

def test():
	exePath = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename))
	dbgmsg("exe path: {}".format(exePath))
	fpath = exePath + "/../_test.xgeo"
	xgeo = XGeo()
	xgeo.load(fpath)
	if not xgeo.isLoaded(): return
	dbgmsg("name: {}".format(xgeo.getName()))
	dbgmsg("path: {}".format(xgeo.getPath()))
	dbgmsg("#mtl: {}".format(xgeo.mtlNum))
	npol = xgeo.polNum
	for i in xrange(npol):
		nvtx = xgeo.pols[i].vtxNum
		vlst = xgeo.pols[i].vtxLst
		#dbgmsg("pol[{}], {} vtx: {}".format(i, nvtx, vlst))
	xgeo.saveLS(exePath + "/_xgeo.ls");

if __name__=="__main__":
	test()
