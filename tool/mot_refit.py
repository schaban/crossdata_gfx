import sys
import hou
import os

def setNodeKey(node, chan, frame, val):
	key = hou.Keyframe()
	key.setExpression("linear()")
	key.setFrame(frame)
	key.setValue(val)
	hou.parm(node.path() + '/' + chan).setKeyframe(key)

def motRefit(srcPath, mkLoop = True, rotTol = 0.25, posTol = 0.0001, grpName = "MOT"):
	chop = hou.node(srcPath)
	if not chop: return
	fps = chop.sampleRate()
	srange = chop.sampleRange()
	fstart = int(srange[0] + 1)
	fend = int(srange[1] + 1)

	tracks = chop.tracks()
	nch = len(tracks)

	if grpName:
		hou.hscript("chgrm " + grpName)
		hou.hscript("chgadd " + grpName)

	for trk in tracks:
		trkName = trk.name()
		chSep = trkName.rfind(":")
		chName = trkName[chSep+1:]
		nodePath = trkName[:chSep]
		node = hou.node(nodePath)
		if node:
			for fno in xrange(fend - fstart + 1):
				val = trk.evalAtFrame(fno)
				setNodeKey(node, chName, fno, val)
			if mkLoop:
				setNodeKey(node, chName, fend+1, trk.evalAtFrame(0))
			tol = posTol
			if chName in ["rx", "ry", "rz"]: tol = rotTol
			chPath = node.path() + '/' + chName
			if grpName: hou.hscript("chgop "+grpName+" add " + chPath)
			prm = hou.parm(chPath)
			print "Refitting", prm.path()
			refitEnd = fend
			if mkLoop: refitEnd = fend + 1
			prm.keyframesRefit(True, tol, True, False, False, 0, 0, True, fstart, refitEnd, hou.parmBakeChop.Off)
		else:
			print "Missing", nodePath


if __name__=="__main__":
	motRefit("/obj/MOTION/OUT")
