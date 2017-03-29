# coding=utf-8
from __future__ import absolute_import
import octoprint.plugin
import serial
import binascii
from threading import Thread
import time

from time import sleep

class OctoremotePlugin(octoprint.plugin.SettingsPlugin,
                       octoprint.plugin.AssetPlugin,
                       octoprint.plugin.TemplatePlugin,
					   octoprint.plugin.StartupPlugin,
                       octoprint.plugin.ShutdownPlugin):

	##~~ SettingsPlugin mixin

	def get_settings_defaults(self):
		return dict(
			comport="COM3",
            baudrate=115200,
            baudrateOpt=[9600,19200,115200],
            extrusionAmount=5,
			retractionAmount=5,
			numberOfTools=3,
			movementSteps=[0.1,1,10,100],
			movementStep1 = 0.1,
			movementStep2 = 1,
			movementStep3 = 10,
			movementStep4 = 100

		)

	def get_config_vars(self):
		return dict(
			comport=self._settings.get(["comport"]),
        	baudrate=self._settings.get(["baudrate"]),
			baudrateOpt=self._settings.get(["baudrateOpt"]),
			extrusionAmount=self._settings.get(["extrusionAmount"]),
			retractionAmount = self._settings.get(["retractionAmount"]),
			numberOfTools=self._settings.get(["numberOfTools"]),
			movementSteps=[self._settings.get(["movementStep1"]),self._settings.get(["movementStep2"]),self._settings.get(["movementStep3"]),self._settings.get(["movementStep4"])]
		)

	def get_template_configs(self):
		return [
			#dict(type="navbar", custom_bindings=False),
			dict(type="settings", custom_bindings=False)
		]


	def on_settings_save(self,data):
		self.stop_com_thread()
		self.start_com_thread()
		#restart the thread
	##~~ AssetPlugin mixin

	def get_assets(self):
		# Define your plugin's asset files to automatically include in the
		# core UI here.
		return dict(
			js=["js/OctoRemote.js"],
			css=["css/OctoRemote.css"],
			less=["less/OctoRemote.less"]
		)

	##~~ Softwareupdate hook

	def get_update_information(self):
		# Define the configuration for your plugin to use with the Software Update
		# Plugin here. See https://github.com/foosel/OctoPrint/wiki/Plugin:-Software-Update
		# for details.
		return dict(
			OctoRemote=dict(
				displayName="Octoremote Plugin",
				displayVersion=self._plugin_version,

				# version check: github repository
				type="github_release",
				user="pkElectronics",
				repo="OctoPrint-Octoremote",
				current=self._plugin_version,

				# update method: pip
				pip="https://github.com/pkElectronics/OctoPrint-Octoremote/archive/{target_version}.zip"
			)
		)
	def on_after_startup(self):
		self.start_com_thread()

	def start_com_thread(self):
		conf = self.get_config_vars()
		self.comthread = SerialThread(self,conf)
		#self.comthread.start()

	def stop_com_thread(self):
		self.comthread.interrupt()
		self.comthread.join()


	def on_shutdown(self):
				self._logger.info("on shutdown")
				self.comthread.interrupted = True
				self.comthread.interrupt()


#Other stuff below
#
#
	def getPrinterObject(self):
		return self._printer
	def getLogger(self):
		return self._logger


	#serial.tools.list_ports listet alle comports auf
# If you want your plugin to be registered within OctoPrint under a different name than what you defined in setup.py
# ("OctoPrint-PluginSkeleton"), you may define that here. Same goes for the other metadata derived from setup.py that
# can be overwritten via __plugin_xyz__ control properties. See the documentation for that.
__plugin_name__ = "Octoremote Plugin"

def __plugin_load__():
	global __plugin_implementation__
	__plugin_implementation__ = OctoremotePlugin()

	global __plugin_hooks__
	__plugin_hooks__ = {
		"octoprint.plugin.softwareupdate.check_config": __plugin_implementation__.get_update_information
	}

class SerialThread(Thread):
	#Fixed responses
	ackResponse = bytearray([0x80, 0x07, 0x01, 0xC3, 0x64, 0x32,0x26])#263264C3
	nackResponse = bytearray([0x80, 0x07, 0x02, 0x79, 0x35, 0x3B, 0xBF])#BF3B3579

	#comport parameters
	portname = ""
	baudrate = 9600

	#thread parameters
	interrupted = False

	#msg parser vars
	msgParsingState = 0
	bytesRead = []
	payload = []
	countBytesRead = 0
	ackPending = False

	#printerSettings
	movementOptions = [0.1,1,10,100]
	toolOptions = ["tool0","tool1","tool2","tool3"]

	movementIndex = 0
	toolIndex = 0

#	comport = "COM3",#
	#baudrate = 9600,
	#extrusionAmount = 5,
	#retractionamount = 5,
	#numberOfTools = 3,
	#movementSteps = [0.1, 1, 10, 100]

	def __init__(self,callbackClass,config):
		Thread.__init__(self)
		self.cbClass = callbackClass
		self.portname = config["comport"]
		self.baudrate = config["baudrate"]
		self.toolcount = config["numberOfTools"]
		if self.toolcount > 4:
			callbackClass.getLogger().info("OctoRemote sanity check: Reverted Toolcount to 4, was"+self.toolcount)
			self.toolcount = 4

		self.extrusionAmount = config["extrusionAmount"]
		self.retractionAmount = config["retractionAmount"]
		self.movementOptions = config["movementSteps"]
		try:
			self.port = serial.Serial(self.portname, baudrate=self.baudrate, timeout=3.0)
		except:
			self.interrupt()
			callbackClass.getLogger().error("Octoremote, could not open comport:"+self.portname)
		callbackClass.getLogger().info("Octoremote Comthread started")
		self.daemon = True
		self.start()



	def run(self):
		self.cbClass.getLogger().info("Thread started")
		self.sendCommandWithPayload(0x20, [self.movementIndex], 1)
		self.sendCommandWithPayload(0x20, [self.toolIndex + 4], 1)

		while not self.interrupted:
			try:
				readbyte = self.port.read(1)
				if self.msgParsingState == 0:
					if readbyte == '\x80':
						self.bytesRead.append(ord(readbyte))
						self.msgParsingState+=1
						self.countBytesRead+=1

				elif self.msgParsingState == 1:
					self.telegramLength = ord(readbyte)
					self.bytesRead.append(ord(readbyte))
					self.msgParsingState+=1
					self.countBytesRead+=1

				elif self.msgParsingState == 2:
					self.command = ord(readbyte)
					self.bytesRead.append(ord(readbyte))
					self.msgParsingState+=1
					self.countBytesRead+=1
					if self.telegramLength == 7:
						self.msgParsingState+=1

				elif self.msgParsingState == 3:
					self.bytesRead.append(ord(readbyte))
					self.payload.append(ord(readbyte))
					self.countBytesRead+=1
					if self.countBytesRead == self.telegramLength-4:
						self.msgParsingState+=1
				elif self.msgParsingState == 4:
					self.crc32 = ord(readbyte)
					self.countBytesRead+=1
					self.msgParsingState+=1

				elif self.msgParsingState == 5:
					self.crc32 |= ord(readbyte)<<8
					self.countBytesRead+=1
					self.msgParsingState+=1

				elif self.msgParsingState == 6:
					self.crc32 |= ord(readbyte)<<16
					self.countBytesRead+=1
					self.msgParsingState+=1

				elif self.msgParsingState == 7:
					self.crc32 |= ord(readbyte)<<24
					self.countBytesRead+=1
					self.msgParsingState+=1
					crc32 = binascii.crc32(bytearray(self.bytesRead)) % (1<<32)
					if crc32 == self.crc32:
						self.performActions(self.command,self.payload)
					else:
						self.sendNack()

					self.msgParsingState = 0
					self.crc32 = 0
					self.countBytesRead = 0
					self.bytesRead = []
					self.payload = []
					self.telegramLength = 0
					self.command = 0
			except:
				pass
	def interrupt(self):
		self.interrupted = True

	def performActions(self,cmd,payload):
		try:
			if cmd == 0x01:
				self.ackPending = False
			elif cmd == 0x02:
				self.resendLastMessage()
			elif cmd == 0x10: #key pressed
				self.sendAck()
				if payload[0] == 0x11:
					self.movementIndex = (self.movementIndex+1)%4
					self.sendCommandWithPayload(0x20,[self.movementIndex],1)
				elif payload[0] == 0x12:
					self.getPrinterObject().jog(dict(x=self.movementOptions[self.movementIndex]))
				elif payload[0] == 0x13:
					self.getPrinterObject().extrude(self.extrusionAmount)
				elif payload[0] == 0x14:
					self.getPrinterObject().jog(dict(z=self.movementOptions[self.movementIndex]))
				elif payload[0] == 0x21:
					self.getPrinterObject().jog(dict(y=-self.movementOptions[self.movementIndex]))
				elif payload[0] == 0x22:
					self.getPrinterObject().home(["x","y"])
				elif payload[0] == 0x23:
					self.getPrinterObject().jog(dict(y=self.movementOptions[self.movementIndex]))
				elif payload[0] == 0x24:
					self.getPrinterObject().home(["z"])
				elif payload[0] == 0x31:
					self.toolIndex = (self.toolIndex + 1) % 4
					self.sendCommandWithPayload(0x20, [self.toolIndex + 4], 1)
					self.cbClass.getPrinterObject().change_tool(self.toolOptions[self.toolIndex])
				elif payload[0] == 0x32:
					self.getPrinterObject().jog(dict(x=-self.movementOptions[self.movementIndex]))
				elif payload[0] == 0x33:
					self.getPrinterObject().extrude(-self.retractionAmount)
				elif payload[0] == 0x34:
					self.getPrinterObject().jog(dict(z=-self.movementOptions[self.movementIndex]))
				elif payload[0] == 0x41:
					self.getPrinterObject().cancel_print()
				elif payload[0] == 0x42:
					self.getPrinterObject().pause_print()
				elif payload[0] == 0x43:
					if self.getPrinterObject().is_paused():
						self.getPrinterObject().resume_print()
					else:
						self.getPrinterObject().start_print()
				elif payload[0] == 0x44:
					self.getPrinterObject()
			elif cmd == 0x11: #key released
				self.stuff = ""
				#self.cbClass._logger.info("KR")
			elif cmd == 0x12: #key longpress
				self.stuff = ""
				#self.cbClass._logger.info("KL")
			else:
				self.stuff = ""
				#self.cbClass._logger.info("FAIL")
		except:
			pass

	def sendAck(self):
		try:
			self.port.write(self.ackResponse)
		except:
			pass
	def sendNack(self):
		try:
			self.port.write(self.nackResponse)
		except:
			pass

	def sendCommandWithPayload(self,cmd,payload,payloadLength):
		try:
			message = []
			message.append(0x80)
			message.append(payloadLength+7)
			message.append(cmd)
			message = message + payload
			bytes = bytearray(message)
			crc32 = binascii.crc32(bytes) % (1<<32)

			message.append(int(crc32 & 0xFF))
			message.append(int(crc32>>8 & 0xFF))
			message.append(int(crc32>>16 & 0xFF))
			message.append(int(crc32>>24 & 0xFF))
			self.lastMessage = bytearray(message)
			self.port.write(self.lastMessage)

			self.ackPending = True
		except:
			pass

	def resendLastMessage(self):
		try:
			self.port.write(self.lastMessage)
		except:
			pass
	def getPrinterObject(self):
		return self.cbClass.getPrinterObject()
#KEypad matrix
 # 0x11 0x12 0x13 0x14
 # 0x21 0x22 0x23 0x24
 # 0x31 0x32 0x33 0x34
 # 0x41 0x42 0x43 0x44

 #keypad functions
# Change Jog Distance	|	Y+			|	Extrude		|	Z+
# X-					|	Home X+Y	|	X+			|	Home Z
# Change Tool			|	Y-			|	Retract		|	Z-
# Stop Print			|	Pause Print	|	Start Print	|	?
