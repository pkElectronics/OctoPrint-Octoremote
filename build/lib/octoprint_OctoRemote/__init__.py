# coding=utf-8
from __future__ import absolute_import

### (Don't forget to remove me)
# This is a basic skeleton for your plugin's __init__.py. You probably want to adjust the class name of your plugin
# as well as the plugin mixins it's subclassing from. This is really just a basic skeleton to get you started,
# defining your plugin as a template plugin, settings and asset plugin. Feel free to add or remove mixins
# as necessary.
#
# Take a look at the documentation on what other plugin mixins are available.

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
			port="COM3",
                        baud=9600,
                        kekse="ja"
		)

	def get_template_vars(self):
		return dict(port=self._settings.get(["port"]),
        baud=self._settings.get(["baud"]),
        kekse=self._settings.get(["kekse"])
		)

	def on_settings_save(data):
		pass #restart the thread
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
                conf = self.get_template_vars()
		self.comthread = SerialThread(self,conf['port'],conf['baud'])
		self.comthread.start()
		self._logger.info("Hello woot!")

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


	def __init__(self,callbackClass,pname,brate):
		Thread.__init__(self)
		self.portname = pname
		self.cbClass = callbackClass
		self.baudrate = brate
		self.port = serial.Serial(self.portname, baudrate=self.baudrate, timeout=3.0)
		callbackClass.getLogger().exception("test")
		self.extrusionAmount = 5;
		self.retractionAmount = 5;


	def run(self):
		self.cbClass.getLogger().info("Thread started")
		self.sendAck()
		while not self.interrupted:
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
	def interrupt(self):
		self.interrupted = True

	def performActions(self,cmd,payload):
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
				self.getPrinterObject().jog(dict(z=self.movementOptions[self.movementIndex]))
			elif payload[0] == 0x14:
				self.getPrinterObject().extrude(self.extrusionAmount)
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

	def sendAck(self):
		self.port.write(self.ackResponse)
	def sendNack(self):
		self.port.write(self.nackResponse)

	def sendCommandWithPayload(self,cmd,payload,payloadLength):
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
		self.cbClass._logger.info([hex(c) for c in self.lastMessage])
		self.ackPending = True

	def resendLastMessage(self):
		self.port.write(self.lastMessage)
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
