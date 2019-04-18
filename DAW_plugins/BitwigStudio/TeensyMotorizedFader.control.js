loadAPI(7);

host.defineController("My", "Teensy Motorized Fader", "1.0", "9106ecdb-a0a8-11e8-82c0-086a0ab8760e");
host.defineMidiPorts(1, 1);
host.addDeviceNameBasedDiscoveryPair(["Teensy Motorized Fader"], ["Teensy Motorized Fader"]);


var CC =
{
	FADERS_FIRST_CC : 1,
	FADERS_LAST_CC	: 8,
	BUTTONS_FIRST_CC : 9,
	BUTTONS_LAST_CC : 16
};

var faderUserControls = [];

function init()
{
	host.getMidiInPort(0).setMidiCallback(onMidi);
	host.getMidiInPort(0).setSysexCallback(onSysex);

	var numFaders = CC.FADERS_LAST_CC - CC.FADERS_FIRST_CC + 1;
	userControls = host.createUserControls(numFaders);
	var cc = CC.FADERS_FIRST_CC;
	for (var userControlId = 0; userControlId < numFaders; userControlId++) {
		var ctl = userControls.getControl(userControlId);
		ctl.setLabel("MotorFader " + userControlId);
		ctl.addValueObserver(128, generateFaderValueObserverCallback(cc));
		faderUserControls[userControlId] = ctl;
		cc++;
	}

}

function generateFaderValueObserverCallback(midi_cc)
{
	return function(value)
	{
		host.getMidiOutPort(0).sendMidi(0xB0, midi_cc, value);
	};
}

function exit()
{
}

function onMidi(status, data1, data2)
{
	var cc = data1;
	var val = data2;
	//printMidi(status, cc, val);

	if (status == 0xB0 && (cc >= CC.FADERS_FIRST_CC && cc <= CC.FADERS_LAST_CC))
	{
		faderUserControls[cc - CC.FADERS_FIRST_CC].set(val, 128);
	}
}

function onSysex(data)
{
}