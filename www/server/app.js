/**
 * GP2040-CE Configurator Development Server
 */

import express from 'express';
import cors from 'cors';
import mapValues from 'lodash/mapValues.js';
import { readFileSync } from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { DEFAULT_KEYBOARD_MAPPING } from '../src/Data/Keyboard.js';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const { pico: picoController } = JSON.parse(
	readFileSync(path.resolve(__dirname, '../src/Data/Controllers.json'), 'utf8'),
);

// Structure pin mappings to include masks and profile label
const createPinMappings = ({ profileLabel = 'Profile', enabled = true }) => {
	let pinMappings = { profileLabel, enabled };

	for (const [key, value] of Object.entries(picoController)) {
		pinMappings[key] = {
			action: value,
			customButtonMask: 0,
			customDpadMask: 0,
		};
	}
	return pinMappings;
};

const port = process.env.PORT || 8080;

const app = express();
app.use(cors());
app.use(express.json());
app.use((req, res, next) => {
	console.log('Request:', req.method, req.url);
	next();
});

app.get('/api/getUsedPins', (req, res) => {
	return res.send({ usedPins: Object.values(picoController) });
});

app.get('/api/resetSettings', (req, res) => {
	return res.send({ success: true });
});

app.get('/api/getDisplayOptions', (req, res) => {
	const data = {
		enabled: 1,
		flipDisplay: 0,
		invertDisplay: 1,
		buttonLayout: 0,
		buttonLayoutRight: 3,
		buttonLayoutOrientation: 0,
		splashMode: 3,
		splashChoice: 0,
		splashDuration: 0,
		buttonLayoutCustomOptions: {
			params: {
				layout: 2,
				startX: 8,
				startY: 28,
				buttonRadius: 8,
				buttonPadding: 2,
			},
			paramsRight: {
				layout: 9,
				startX: 8,
				startY: 28,
				buttonRadius: 8,
				buttonPadding: 2,
			},
		},

		displaySaverTimeout: 0,
		displaySaverMode: 0,
		turnOffWhenSuspended: 0,
		inputMode: 1,
		turboMode: 1,
		dpadMode: 1,
		socdMode: 1,
		macroMode: 1,
		profileMode: 0,
		inputHistoryEnabled: 0,
		inputHistoryLength: 21,
		inputHistoryCol: 0,
		inputHistoryRow: 7,
	};
	console.log('data', data);
	return res.send(data);
});

app.get('/api/getSplashImage', (req, res) => {
	const data = {
		splashImage: Array(16 * 64).fill(255),
	};
	console.log('data', data);
	return res.send(data);
});

app.get('/api/getAnimationProtoOptions', (req, res) => {
	return res.send({
		AnimationOptions: {
			brightness: 5,
			baseProfileIndex: 0,
			customColors: [255],
			profiles: [
				{
					bEnabled: 1,
					baseNonPressedEffect: 1,
					basePressedEffect: 0,
					buttonPressHoldTimeInMs: 500,
					buttonPressFadeOutTimeInMs: 500,
					nonPressedSpecialColor: 0xffff00,
					bUseCaseLightsInSpecialMoves: 0,
					bUseCaseLightsInPressedAnimations: 0,
					baseCaseEffect: 0,
					pressedSpecialColor: 0,
					notPressedStaticColors: [
						2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						2, 2, 2, 2, 2, 2, 2, 2, 2,
					],
					pressedStaticColors: [
						4, 6, 10, 12, 4, 6, 10, 12, 4, 6, 10, 12, 4, 6, 10, 12, 4, 6, 10,
						12, 4, 6, 10, 12, 4, 6, 10, 12, 4, 6, 10, 12,
					],
					caseStaticColors: [
						1, 5, 2, 1, 2, 9, 2, 2, 2, 2, 3, 2, 3, 2, 1, 2, 2, 2, 2, 2, 2, 2, 2,
						2, 9, 2, 5, 3, 2, 2, 7, 2, 2, 2, 11, 10, 4, 2, 2, 2,
					],
				},
				{
					bEnabled: 1,
					baseNonPressedEffect: 0,
					basePressedEffect: 3,
					buttonPressHoldTimeInMs: 500,
					buttonPressFadeOutTimeInMs: 500,
					nonPressedSpecialColor: 255,
					bUseCaseLightsInSpecialMoves: 1,
					bUseCaseLightsInPressedAnimations: 1,
					baseCaseEffect: 0,
					pressedSpecialColor: 0x80ff00,
					notPressedStaticColors: [
						6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
						6, 6, 6, 6, 6, 6, 6, 6, 6,
					],
					pressedStaticColors: [
						2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
						2, 2, 2, 2, 2, 2, 2, 2, 2,
					],
					caseStaticColors: [
						13, 1, 1, 1, 13, 1, 1, 1, 13, 1, 1, 1, 13, 1, 1, 1, 13, 1, 1, 1, 13,
						1, 1, 1, 13, 1, 1, 1, 13, 1, 1, 1, 13, 1, 1, 1, 13, 1, 1, 1,
					],
				},
			],
		},
	});
});

app.get('/api/getGamepadOptions', (req, res) => {
	return res.send({
		dpadMode: 0,
		inputMode: 4,
		socdMode: 2,
		switchTpShareForDs4: 0,
		forcedSetupMode: 0,
		lockHotkeys: 0,
		fourWayMode: 0,
		fnButtonPin: -1,
		profileNumber: 2,
		debounceDelay: 5,
		inputModeB1: 1,
		inputModeB2: 0,
		inputModeB3: 2,
		inputModeB4: 4,
		inputModeL1: -1,
		inputModeL2: -1,
		inputModeR1: -1,
		inputModeR2: 3,
		ps4AuthType: 0,
		ps5AuthType: 0,
		xinputAuthType: 0,
		ps4ControllerIDMode: 0,
		usbDescOverride: 0,
		usbDescProduct: 'GP2040-CE (Custom)',
		usbDescManufacturer: 'Open Stick Community',
		usbDescVersion: '1.0',
		usbOverrideID: 0,
		usbVendorID: '10C4',
		usbProductID: '82C0',
		miniMenuGamepadInput: 1,
		hotkey01: {
			auxMask: 32768,
			buttonsMask: 66304,
			action: 4,
		},
		hotkey02: {
			auxMask: 0,
			buttonsMask: 131840,
			action: 1,
		},
		hotkey03: {
			auxMask: 0,
			buttonsMask: 262912,
			action: 2,
		},
		hotkey04: {
			auxMask: 0,
			buttonsMask: 525056,
			action: 3,
		},
		hotkey05: {
			auxMask: 0,
			buttonsMask: 70144,
			action: 6,
		},
		hotkey06: {
			auxMask: 0,
			buttonsMask: 135680,
			action: 7,
		},
		hotkey07: {
			auxMask: 0,
			buttonsMask: 266752,
			action: 8,
		},
		hotkey08: {
			auxMask: 0,
			buttonsMask: 528896,
			action: 10,
		},
		hotkey09: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey10: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey11: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey12: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey13: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey14: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey15: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
		hotkey16: {
			auxMask: 0,
			buttonsMask: 0,
			action: 0,
		},
	});
});

app.get('/api/getLedOptions', (req, res) => {
	return res.send({
		brightnessMaximum: 255,
		brightnessSteps: 5,
		dataPin: 22,
		ledFormat: 0,
		ledLayout: 1,
		ledsPerButton: 2,
		ledButtonMap: {
			Up: 3,
			Down: 1,
			Left: 0,
			Right: 2,
			B1: 8,
			B2: 9,
			B3: 4,
			B4: 5,
			L1: 7,
			R1: 6,
			L2: 11,
			R2: 10,
			S1: null,
			S2: null,
			L3: null,
			R3: null,
			A1: null,
			A2: null,
		},
		usedPins: Object.values(picoController),
		pledType: 1,
		pledPin1: 12,
		pledPin2: 13,
		pledPin3: 14,
		pledPin4: 15,
		pledIndex1: 12,
		pledIndex2: 13,
		pledIndex3: 14,
		pledIndex4: 15,
		pledColor: 65280,
		caseRGBType: 0,
		caseRGBIndex: -1,
		caseRGBCount: 0,
		turnOffWhenSuspended: 0,
	});
});

app.get('/api/getPinMappings', (req, res) => {
	return res.send(createPinMappings({ profileLabel: 'Profile 1' }));
});

app.get('/api/getKeyMappings', (req, res) =>
	res.send(mapValues(DEFAULT_KEYBOARD_MAPPING)),
);

app.get('/api/getPeripheralOptions', (req, res) => {
	return res.send({
		peripheral: {
			i2c0: {
				enabled: 1,
				sda: 0,
				scl: 1,
				speed: 400000,
			},
			i2c1: {
				enabled: 0,
				sda: -1,
				scl: -1,
				speed: 400000,
			},
			spi0: {
				enabled: 1,
				rx: 16,
				cs: 17,
				sck: 18,
				tx: 19,
			},
			spi1: {
				enabled: 0,
				rx: -1,
				cs: -1,
				sck: -1,
				tx: -1,
			},
			usb0: {
				enabled: 1,
				dp: 28,
				enable5v: -1,
				order: 0,
			},
		},
	});
});

app.get('/api/getWiiControls', (req, res) =>
	res.send({
		'nunchuk.analogStick.x.axisType': 1,
		'nunchuk.analogStick.y.axisType': 2,
		'nunchuk.buttonC': 1,
		'nunchuk.buttonZ': 2,
		'classic.analogLeftStick.x.axisType': 1,
		'classic.analogLeftStick.y.axisType': 2,
		'classic.analogRightStick.x.axisType': 3,
		'classic.analogRightStick.y.axisType': 4,
		'classic.analogLeftTrigger.axisType': 7,
		'classic.analogRightTrigger.axisType': 8,
		'classic.buttonA': 2,
		'classic.buttonB': 1,
		'classic.buttonX': 8,
		'classic.buttonY': 4,
		'classic.buttonL': 64,
		'classic.buttonR': 128,
		'classic.buttonZL': 16,
		'classic.buttonZR': 32,
		'classic.buttonMinus': 256,
		'classic.buttonHome': 4096,
		'classic.buttonPlus': 512,
		'classic.buttonUp': 65536,
		'classic.buttonDown': 131072,
		'classic.buttonLeft': 262144,
		'classic.buttonRight': 524288,
		'guitar.analogStick.x.axisType': 1,
		'guitar.analogStick.y.axisType': 2,
		'guitar.analogWhammyBar.axisType': 14,
		'guitar.buttonOrange': 64,
		'guitar.buttonRed': 2,
		'guitar.buttonBlue': 4,
		'guitar.buttonGreen': 1,
		'guitar.buttonYellow': 8,
		'guitar.buttonPedal': 128,
		'guitar.buttonMinus': 256,
		'guitar.buttonPlus': 512,
		'guitar.buttonStrumUp': 65536,
		'guitar.buttonStrumDown': 131072,
		'drum.analogStick.x.axisType': 1,
		'drum.analogStick.y.axisType': 2,
		'drum.buttonOrange': 64,
		'drum.buttonRed': 2,
		'drum.buttonBlue': 8,
		'drum.buttonGreen': 1,
		'drum.buttonYellow': 4,
		'drum.buttonPedal': 128,
		'drum.buttonMinus': 256,
		'drum.buttonPlus': 512,
		'turntable.analogStick.x.axisType': 1,
		'turntable.analogStick.y.axisType': 2,
		'turntable.analogLeftTurntable.axisType': 13,
		'turntable.analogRightTurntable.axisType': 15,
		'turntable.analogFader.axisType': 7,
		'turntable.analogEffects.axisType': 8,
		'turntable.buttonLeftGreen': 262144,
		'turntable.buttonLeftRed': 65536,
		'turntable.buttonLeftBlue': 524288,
		'turntable.buttonRightGreen': 4,
		'turntable.buttonRightRed': 8,
		'turntable.buttonRightBlue': 2,
		'turntable.buttonEuphoria': 32,
		'turntable.buttonMinus': 256,
		'turntable.buttonPlus': 512,
		'taiko.buttonDonLeft': 262144,
		'taiko.buttonKatLeft': 64,
		'taiko.buttonDonRight': 1,
		'taiko.buttonKatRight': 128,
	}),
);

app.get('/api/getProfileOptions', (req, res) => {
	return res.send({
		alternativePinMappings: [
			createPinMappings({ profileLabel: 'Profile 2' }),
			createPinMappings({ profileLabel: 'Profile 3', enabled: false }),
		],
	});
});

app.get('/api/getAddonsOptions', (req, res) => {
	return res.send({
		turboPinLED: -1,
		sliderModeZero: 0,
		turboShotCount: 20,
		reversePin: -1,
		reversePinLED: -1,
		reverseActionUp: 1,
		reverseActionDown: 1,
		reverseActionLeft: 1,
		reverseActionRight: 1,
		onBoardLedMode: 0,
		dualDirDpadMode: 0,
		dualDirCombineMode: 0,
		dualDirFourWayMode: 0,
		tilt1Pin: -1,
		factorTilt1LeftX: 0,
		factorTilt1LeftY: 0,
		factorTilt1RightX: 0,
		factorTilt1RightY: 0,
		tilt2Pin: -1,
		factorTilt2LeftX: 0,
		factorTilt2LeftY: 0,
		factorTilt2RightX: 0,
		factorTilt2RightY: 0,
		tiltLeftAnalogUpPin: -1,
		tiltLeftAnalogDownPin: -1,
		tiltLeftAnalogLeftPin: -1,
		tiltLeftAnalogRightPin: -1,
		tiltRightAnalogUpPin: -1,
		tiltRightAnalogDownPin: -1,
		tiltRightAnalogLeftPin: -1,
		tiltRightAnalogRightPin: -1,
		tiltSOCDMode: 0,
		analogAdc1PinX: -1,
		analogAdc1PinY: -1,
		analogAdc1Mode: 1,
		analogAdc1Invert: 0,
		analogAdc2PinX: -1,
		analogAdc2PinY: -1,
		analogAdc2Mode: 2,
		analogAdc2Invert: 0,
		forced_circularity: 0,
		forced_circularity2: 0,
		inner_deadzone: 5,
		inner_deadzone2: 5,
		outer_deadzone: 95,
		outer_deadzone2: 95,
		auto_calibrate: 0,
		auto_calibrate2: 0,
		analog_smoothing: 0,
		analog_smoothing2: 0,
		smoothing_factor: 5,
		smoothing_factor2: 5,
		analog_error: 1000,
		analog_error2: 1000,
		bootselButtonMap: 0,
		buzzerPin: -1,
		buzzerEnablePin: -1,
		buzzerVolume: 100,
		drv8833RumbleLeftMotorPin: -1,
		drv8833RumbleRightMotorPin: -1,
		drv8833RumbleMotorSleepPin: -1,
		drv8833RumblePWMFrequency: 10000,
		drv8833RumbleDutyMin: 0,
		drv8833RumbleDutyMax: 100,
		focusModePin: -1,
		focusModeButtonLockMask: 0,
		focusModeButtonLockEnabled: 0,
		shmupMode: 0,
		shmupMixMode: 0,
		shmupAlwaysOn1: 0,
		shmupAlwaysOn2: 0,
		shmupAlwaysOn3: 0,
		shmupAlwaysOn4: 0,
		pinShmupBtn1: -1,
		pinShmupBtn2: -1,
		pinShmupBtn3: -1,
		pinShmupBtn4: -1,
		shmupBtnMask1: 0,
		shmupBtnMask2: 0,
		shmupBtnMask3: 0,
		shmupBtnMask4: 0,
		pinShmupDial: -1,
		turboLedType: 1,
		turboLedIndex: 16,
		turboLedColor: 16711680,
		sliderSOCDModeDefault: 1,
		snesPadClockPin: -1,
		snesPadLatchPin: -1,
		snesPadDataPin: -1,
		keyboardHostMap: DEFAULT_KEYBOARD_MAPPING,
		keyboardHostMouseLeft: 0,
		keyboardHostMouseMiddle: 0,
		keyboardHostMouseRight: 0,
		AnalogInputEnabled: 1,
		BoardLedAddonEnabled: 1,
		FocusModeAddonEnabled: 1,
		focusModeMacroLockEnabled: 0,
		BuzzerSpeakerAddonEnabled: 1,
		BootselButtonAddonEnabled: 1,
		DualDirectionalInputEnabled: 1,
		TiltInputEnabled: 1,
		I2CAnalog1219InputEnabled: 1,
		KeyboardHostAddonEnabled: 1,
		PlayerNumAddonEnabled: 1,
		ReverseInputEnabled: 1,
		SliderSOCDInputEnabled: 1,
		TurboInputEnabled: 1,
		WiiExtensionAddonEnabled: 1,
		SNESpadAddonEnabled: 1,
		Analog1256Enabled: 1,
		analog1256Block: 0,
		analog1256CsPin: -1,
		analog1256DrdyPin: -1,
		analog1256AnalogMax: 3.3,
		analog1256EnableTriggers: false,
		encoderOneEnabled: 0,
		encoderOnePinA: -1,
		encoderOnePinB: -1,
		encoderOneMode: 0,
		encoderOnePPR: 24,
		encoderOneResetAfter: 0,
		encoderOneAllowWrapAround: false,
		encoderOneMultiplier: 1,
		encoderTwoEnabled: 0,
		encoderTwoPinA: -1,
		encoderTwoPinB: -1,
		encoderTwoMode: 0,
		encoderTwoPPR: 24,
		encoderTwoResetAfter: 0,
		encoderTwoAllowWrapAround: false,
		encoderTwoMultiplier: 1,
		RotaryAddonEnabled: 1,
		PCF8575AddonEnabled: 1,
		DRV8833RumbleAddonEnabled: 1,
		ReactiveLEDAddonEnabled: 1,
		GamepadUSBHostAddonEnabled: 1,
		usedPins: Object.values(picoController),
	});
});

app.get('/api/getLightsDataOptions', (req, res) => {
	return res.send({
		LightData: {
			Lights: [
				{
					firstLedIndex: 0,
					numLedsOnLight: 1,
					xCoord: 0,
					yCoord: 2,
					GPIOPinorCaseChainIndex: 5,
					lightType: 0,
				},
				{
					firstLedIndex: 1,
					numLedsOnLight: 1,
					xCoord: 2,
					yCoord: 2,
					GPIOPinorCaseChainIndex: 3,
					lightType: 0,
				},
				{
					firstLedIndex: 2,
					numLedsOnLight: 1,
					xCoord: 4,
					yCoord: 3,
					GPIOPinorCaseChainIndex: 4,
					lightType: 0,
				},
				{
					firstLedIndex: 3,
					numLedsOnLight: 1,
					xCoord: 6,
					yCoord: 7,
					GPIOPinorCaseChainIndex: 2,
					lightType: 0,
				},
				{
					firstLedIndex: 4,
					numLedsOnLight: 1,
					xCoord: 6,
					yCoord: 2,
					GPIOPinorCaseChainIndex: 10,
					lightType: 0,
				},
				{
					firstLedIndex: 5,
					numLedsOnLight: 1,
					xCoord: 8,
					yCoord: 1,
					GPIOPinorCaseChainIndex: 11,
					lightType: 0,
				},
				{
					firstLedIndex: 6,
					numLedsOnLight: 1,
					xCoord: 10,
					yCoord: 1,
					GPIOPinorCaseChainIndex: 12,
					lightType: 0,
				},
				{
					firstLedIndex: 7,
					numLedsOnLight: 1,
					xCoord: 12,
					yCoord: 1,
					GPIOPinorCaseChainIndex: 13,
					lightType: 0,
				},
				{
					firstLedIndex: 8,
					numLedsOnLight: 1,
					xCoord: 6,
					yCoord: 4,
					GPIOPinorCaseChainIndex: 6,
					lightType: 0,
				},
				{
					firstLedIndex: 9,
					numLedsOnLight: 1,
					xCoord: 8,
					yCoord: 3,
					GPIOPinorCaseChainIndex: 7,
					lightType: 0,
				},
				{
					firstLedIndex: 10,
					numLedsOnLight: 1,
					xCoord: 10,
					yCoord: 3,
					GPIOPinorCaseChainIndex: 8,
					lightType: 0,
				},
				{
					firstLedIndex: 11,
					numLedsOnLight: 1,
					xCoord: 12,
					yCoord: 3,
					GPIOPinorCaseChainIndex: 9,
					lightType: 0,
				},
				{
					firstLedIndex: 12,
					numLedsOnLight: 1,
					xCoord: 3,
					yCoord: 0,
					GPIOPinorCaseChainIndex: 27,
					lightType: 0,
				},
				{
					firstLedIndex: 13,
					numLedsOnLight: 1,
					xCoord: 6,
					yCoord: 0,
					GPIOPinorCaseChainIndex: 18,
					lightType: 0,
				},
				{
					firstLedIndex: 14,
					numLedsOnLight: 1,
					xCoord: 8,
					yCoord: 5,
					GPIOPinorCaseChainIndex: 19,
					lightType: 0,
				},
				{
					firstLedIndex: 15,
					numLedsOnLight: 1,
					xCoord: 3,
					yCoord: 6,
					GPIOPinorCaseChainIndex: 26,
					lightType: 0,
				},
			],
		},
	});
});

app.get('/api/getExpansionPins', (req, res) => {
	return res.send({
		pins: {
			pcf8575: [
				{
					pin00: { option: 2, direction: 0 },
					pin01: { option: -10, direction: 0 },
					pin02: { option: -10, direction: 0 },
					pin03: { option: -10, direction: 0 },
					pin04: { option: -10, direction: 0 },
					pin05: { option: -10, direction: 0 },
					pin06: { option: -10, direction: 0 },
					pin07: { option: -10, direction: 0 },
					pin08: { option: -10, direction: 0 },
					pin09: { option: -10, direction: 0 },
					pin10: { option: -10, direction: 0 },
					pin11: { option: -10, direction: 0 },
					pin12: { option: -10, direction: 0 },
					pin13: { option: -10, direction: 0 },
					pin14: { option: -10, direction: 0 },
					pin15: { option: -10, direction: 0 },
				},
			],
		},
	});
});

app.get('/api/getMacroAddonOptions', (req, res) => {
	return res.send({
		macroList: [
			{
				enabled: 1,
				exclusive: 1,
				interruptible: 1,
				showFrames: 1,
				macroType: 1,
				useMacroTriggerButton: 0,
				macroTriggerButton: 0,
				macroLabel: 'Shoryuken',
				macroInputs: [
					{ buttonMask: 1 << 19, duration: 16666, waitDuration: 0 },
					{ buttonMask: 1 << 17, duration: 16666, waitDuration: 0 },
					{
						buttonMask: (1 << 17) | (1 << 19) | (1 << 3),
						duration: 16666,
						waitDuration: 0,
					},
				],
			},
			{
				enabled: 0,
				exclusive: 1,
				interruptible: 1,
				showFrames: 1,
				macroType: 1,
				useMacroTriggerButton: 0,
				macroTriggerButton: 0,
				macroLabel: '',
				macroInputs: [],
			},
			{
				enabled: 0,
				exclusive: 1,
				interruptible: 1,
				showFrames: 1,
				macroType: 1,
				useMacroTriggerButton: 0,
				macroTriggerButton: 0,
				macroLabel: '',
				macroInputs: [],
			},
			{
				enabled: 0,
				exclusive: 1,
				interruptible: 1,
				showFrames: 1,
				macroType: 1,
				useMacroTriggerButton: 0,
				macroTriggerButton: 0,
				macroLabel: '',
				macroInputs: [],
			},
			{
				enabled: 0,
				exclusive: 1,
				interruptible: 1,
				showFrames: 1,
				macroType: 1,
				useMacroTriggerButton: 0,
				macroTriggerButton: 0,
				macroLabel: '',
				macroInputs: [],
			},
			{
				enabled: 0,
				exclusive: 1,
				interruptible: 1,
				showFrames: 1,
				macroType: 1,
				useMacroTriggerButton: 0,
				macroTriggerButton: 0,
				macroLabel: '',
				macroInputs: [],
			},
		],
		macroBoardLedEnabled: 1,
	});
});

app.get('/api/getFirmwareVersion', (req, res) => {
	return res.send({
		boardConfigLabel: 'Pico',
		boardConfigFileName: `GP2040_local-dev-server_Pico`,
		boardConfig: 'Pico',
		version: 'local-dev-server',
	});
});

app.get('/api/getButtonLayouts', (req, res) => {
	return res.send({
		ledLayout: {
			id: 27,
			indexUp: 3,
			indexDown: 1,
			indexLeft: 0,
			indexRight: 2,
			indexB1: 8,
			indexB2: 9,
			indexB3: 4,
			indexB4: 5,
			indexL1: 7,
			indexR1: 6,
			indexL2: 11,
			indexR2: 10,
			indexS1: -1,
			indexS2: -1,
			indexL3: 13,
			indexR3: 14,
			indexA1: 12,
			indexA2: 15,
		},
		displayLayouts: {
			buttonLayoutId: 27,
			buttonLayout: {
				0: {
					elementType: 4,
					parameters: {
						x1: 8,
						y1: 20,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 5,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				1: {
					elementType: 4,
					parameters: {
						x1: 26,
						y1: 20,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 3,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				2: {
					elementType: 4,
					parameters: {
						x1: 41,
						y1: 29,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 4,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				3: {
					elementType: 4,
					parameters: {
						x1: 48,
						y1: 53,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 2,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
			},
			buttonLayoutRightId: 31,
			buttonLayoutRight: {
				0: {
					elementType: 4,
					parameters: {
						x1: 57,
						y1: 20,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 10,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				1: {
					elementType: 4,
					parameters: {
						x1: 75,
						y1: 16,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 11,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				2: {
					elementType: 4,
					parameters: {
						x1: 93,
						y1: 16,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 12,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				3: {
					elementType: 4,
					parameters: {
						x1: 111,
						y1: 20,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 13,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				4: {
					elementType: 4,
					parameters: {
						x1: 57,
						y1: 38,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 6,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				5: {
					elementType: 4,
					parameters: {
						x1: 75,
						y1: 34,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 7,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				6: {
					elementType: 4,
					parameters: {
						x1: 93,
						y1: 34,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 8,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
				7: {
					elementType: 4,
					parameters: {
						x1: 111,
						y1: 38,
						x2: 8,
						y2: 8,
						stroke: 1,
						fill: 1,
						value: 9,
						shape: 0,
						angleStart: 0,
						angleEnd: 0,
					},
				},
			},
		},
	});
});

app.get('/api/getButtonLayoutDefs', (req, res) => {
	return res.send({
		buttonLayout: {
			BUTTON_LAYOUT_STICK: 0,
			BUTTON_LAYOUT_STICKLESS: 1,
			BUTTON_LAYOUT_BUTTONS_ANGLED: 2,
			BUTTON_LAYOUT_BUTTONS_BASIC: 3,
			BUTTON_LAYOUT_KEYBOARD_ANGLED: 4,
			BUTTON_LAYOUT_KEYBOARDA: 5,
			BUTTON_LAYOUT_DANCEPADA: 6,
			BUTTON_LAYOUT_TWINSTICKA: 7,
			BUTTON_LAYOUT_BLANKA: 8,
			BUTTON_LAYOUT_VLXA: 9,
			BUTTON_LAYOUT_FIGHTBOARD_STICK: 10,
			BUTTON_LAYOUT_FIGHTBOARD_MIRRORED: 11,
			BUTTON_LAYOUT_CUSTOMA: 12,
			BUTTON_LAYOUT_OPENCORE0WASDA: 13,
			BUTTON_LAYOUT_STICKLESS_13: 14,
			BUTTON_LAYOUT_STICKLESS_16: 15,
			BUTTON_LAYOUT_STICKLESS_14: 16,
			BUTTON_LAYOUT_DANCEPAD_DDR_LEFT: 17,
			BUTTON_LAYOUT_DANCEPAD_DDR_SOLO: 18,
			BUTTON_LAYOUT_DANCEPAD_PIU_LEFT: 19,
			BUTTON_LAYOUT_POPN_A: 20,
			BUTTON_LAYOUT_TAIKO_A: 21,
			BUTTON_LAYOUT_BM_TURNTABLE_A: 22,
			BUTTON_LAYOUT_BM_5KEY_A: 23,
			BUTTON_LAYOUT_BM_7KEY_A: 24,
			BUTTON_LAYOUT_GITADORA_FRET_A: 25,
			BUTTON_LAYOUT_GITADORA_STRUM_A: 26,
			BUTTON_LAYOUT_BOARD_DEFINED_A: 27,
			BUTTON_LAYOUT_BANDHERO_FRET_A: 28,
			BUTTON_LAYOUT_BANDHERO_STRUM_A: 29,
			BUTTON_LAYOUT_6GAWD_A: 30,
			BUTTON_LAYOUT_6GAWD_ALLBUTTON_A: 31,
			BUTTON_LAYOUT_6GAWD_ALLBUTTONPLUS_A: 32,
			BUTTON_LAYOUT_STICKLESS_R16: 33,
		},
		buttonLayoutRight: {
			BUTTON_LAYOUT_ARCADE: 0,
			BUTTON_LAYOUT_STICKLESSB: 1,
			BUTTON_LAYOUT_BUTTONS_ANGLEDB: 2,
			BUTTON_LAYOUT_VEWLIX: 3,
			BUTTON_LAYOUT_VEWLIX7: 4,
			BUTTON_LAYOUT_CAPCOM: 5,
			BUTTON_LAYOUT_CAPCOM6: 6,
			BUTTON_LAYOUT_SEGA2P: 7,
			BUTTON_LAYOUT_NOIR8: 8,
			BUTTON_LAYOUT_KEYBOARDB: 9,
			BUTTON_LAYOUT_DANCEPADB: 10,
			BUTTON_LAYOUT_TWINSTICKB: 11,
			BUTTON_LAYOUT_BLANKB: 12,
			BUTTON_LAYOUT_VLXB: 13,
			BUTTON_LAYOUT_FIGHTBOARD: 14,
			BUTTON_LAYOUT_FIGHTBOARD_STICK_MIRRORED: 15,
			BUTTON_LAYOUT_CUSTOMB: 16,
			BUTTON_LAYOUT_KEYBOARD8B: 17,
			BUTTON_LAYOUT_OPENCORE0WASDB: 18,
			BUTTON_LAYOUT_STICKLESS_13B: 19,
			BUTTON_LAYOUT_STICKLESS_16B: 20,
			BUTTON_LAYOUT_STICKLESS_14B: 21,
			BUTTON_LAYOUT_DANCEPAD_DDR_RIGHT: 22,
			BUTTON_LAYOUT_DANCEPAD_PIU_RIGHT: 23,
			BUTTON_LAYOUT_POPN_B: 24,
			BUTTON_LAYOUT_TAIKO_B: 25,
			BUTTON_LAYOUT_BM_TURNTABLE_B: 26,
			BUTTON_LAYOUT_BM_5KEY_B: 27,
			BUTTON_LAYOUT_BM_7KEY_B: 28,
			BUTTON_LAYOUT_GITADORA_FRET_B: 29,
			BUTTON_LAYOUT_GITADORA_STRUM_B: 30,
			BUTTON_LAYOUT_BOARD_DEFINED_B: 31,
			BUTTON_LAYOUT_BANDHERO_FRET_B: 32,
			BUTTON_LAYOUT_BANDHERO_STRUM_B: 33,
			BUTTON_LAYOUT_6GAWD_B: 34,
			BUTTON_LAYOUT_6GAWD_ALLBUTTON_B: 35,
			BUTTON_LAYOUT_6GAWD_ALLBUTTONPLUS_B: 36,
			BUTTON_LAYOUT_STICKLESS_R16B: 37,
		},
	});
});

app.get('/api/getReactiveLEDs', (req, res) => {
	return res.send({
		leds: [
			{ pin: -1, action: -10, modeDown: 0, modeUp: 1 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
			{ pin: -1, action: -10, modeDown: 1, modeUp: 0 },
		],
	});
});

app.get('/api/reboot', (req, res) => {
	return res.send({});
});

app.get('/api/getMemoryReport', (req, res) => {
	return res.send({
		totalFlash: 2048 * 1024,
		usedFlash: 1048 * 1024,
		physicalFlash: 2048 * 1024,
		staticAllocs: 200,
		totalHeap: 2048 * 1024,
		usedHeap: 1048 * 1024,
	});
});

app.get('/api/getHeldPins', async (req, res) => {
	await new Promise((resolve) => setTimeout(resolve, 2000));
	return res.send({
		heldPins: [7],
	});
});

app.get('/api/abortGetHeldPins', async (req, res) => {
	return res.send();
});

app.post('/api/*', (req, res) => {
	console.log(req.body);
	return res.send(req.body);
});

app.listen(port, () => {
	console.log(`Dev app listening at http://localhost:${port}`);
});
