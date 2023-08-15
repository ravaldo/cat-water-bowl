import serial, time, datetime, smtplib, logging, email, traceback


ser = serial.Serial('COM3', 57600)

emailServer = ("smtp.live.com", 587)
emailAcc = ("me@hotmail.com", "password")
sendTo = ["me@gmail.com"]


def emailTLS(send_to, subject):
	try:
		assert type(send_to)==list
		msg = email.message_from_string("")
		msg['From'] = "catcafe@home.com"
		msg['To'] = ", ".join(send_to)
		msg['Subject'] = subject
		
		s = smtplib.SMTP(emailServer[0], emailServer[1])
		s.ehlo()
		s.starttls()
		s.ehlo()
		s.login(emailAcc[0], emailAcc[1])
		s.sendmail(emailAcc[0], send_to, msg.as_string())
		s.quit()
	except:
		print "failed to send email: ", subject
		traceback.print_exc()

	
def timestamp():
	return datetime.datetime.now().strftime("%Y/%m/%d %H:%M:%S")


def radioRecieved():
	logF = logging.getLogger('fountain_logger')
	handlerF = logging.FileHandler('fountain.log')
	handlerF.setFormatter(logging.Formatter("%(asctime)s: %(message)s"))
	logF.addHandler(handlerF)
	logF.setLevel(logging.INFO)
	
	while True:
		msg = ser.readline().strip()
		logF.info(msg)
		emailTLS(sendTo, msg)
		print(f"{timestamp()} : {msg}")

		
radioRecieved()
