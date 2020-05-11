import serial
import time

formatterNotes = lambda x: "%04d#" % x
formatterDura = lambda x: "%02d*" % x
formatterTLen = lambda x: "%03d$" % x

songCount = 2
titles = ["Twinkle Star@"]
notes = [[261, 261, 392, 392, 440, 440, 392,
          349, 349, 330, 330, 294, 294, 261,
          392, 392, 349, 349, 330, 330, 294,
          392, 392, 349, 349, 330, 330, 294,
          261, 261, 392, 392, 440, 440, 392,
          349, 349, 330, 330, 294, 294, 261]]
dura = [[2, 2, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2,
         2, 2, 2, 2, 2, 2, 2]]
############################################
titles.append(["Lil Bee@"])

notes.append([392,330,330,349,294,294,261,
			  294,330,349,392,392,392,392,
			  330,330,349,294,294,261,330,
			  392,392,330,294,294,294,294,
			  294,330,349,330,330,330,330,
			  330,349,392,392,330,330,349,
			  294,294,261,330,392,392,261])

dura.append([1,1,2,1,1,2,1,
			1,1,1,1,1,2,1,
			1,2,1,1,2,1,1,
			1,1,4,1,1,1,1,
			1,1,2,1,1,1,1,
			1,1,2,1,1,2,1,
			1,2,1,1,1,1,4])
############################################

waitTime = 0.1
serdev = '/dev/ttyACM0'
s = serial.Serial(serdev)

print("Sending signal ...")
for j in range(0, songCount):
	
	for i in range(0, len(notes[j])):
		s.write(bytes(formatterNotes(notes[j][i]), 'UTF-8'))
	s.write(bytes("!", 'UTF-8'))

	for i in range(0, len(dura[j])):
		s.write(bytes(formatterDura(dura[j][i]), 'UTF-8'))
	s.write(bytes("!", 'UTF-8'))

	s.write(bytes(formatterTLen(len(notes[j])), 'UTF-8'))
	s.write(bytes("!", 'UTF-8'))

	for i in range(0,len(titles[j])):
		s.write(bytes(titles[j][i], 'UTF-8'))
s.close()
print("Done")
