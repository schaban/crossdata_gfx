import sys
import imp
import inspect

import lwsdk as lw
from lwsdk.pris.modeler import *

def libLoad(libName, libPath):
	libFile, libFname, libDescr = imp.find_module(libName, [libPath])
	return imp.load_module(libName, libFile, libFname, libDescr)

def libImpAll(libName, libPath):
	libLoad(libName, libPath)
	return "from " + libName + " import *"

exePath = os.path.dirname(os.path.abspath(inspect.getframeinfo(inspect.currentframe()).filename))
exec(libImpAll("xgeo_load", exePath))


fpath = exePath + "/../_test.xgeo"
xgeo = XGeo()
xgeo.load(fpath)
npnt = xgeo.pntNum
npol = xgeo.polNum

print "Importing XGEO:", xgeo.getName()

if not init(lw.ModCommand()): raise Exception("Modeler command error")

mon = lw.DynaMonitorFuncs().create("xgeo", "xgeo")
if mon: mon.init(mon.data, npnt + npol)

if editbegin():
	pts = []
	for i in xrange(npnt):
		pos = xgeo.pnts[i]
		pnt = addpoint((pos[0], pos[1], -pos[2])) # RH->LH
		pts.append(pnt)
		if mon: mon.step(mon.data, 1)

	for i in xrange(npol):
		nvtx = xgeo.pols[i].vtxNum
		vlst = xgeo.pols[i].vtxLst
		v = []
		for j in xrange(nvtx): v.append(pts[vlst[j]])
		addpolygon(v)
		if mon: mon.step(mon.data, 1)

	editend(lwsdk.EDERR_NONE)
else: raise Exception("editbegin error")

if mon:
	mon.done(mon.data)
	lw.DynaMonitorFuncs().destroy(mon)
