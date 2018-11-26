// Command-line utility that converts an audio file to an .mp4 video file at the
// same location visualizing the audio as MFCC vectors.

const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const writeFile = promisify(require('fs').writeFile);
const { createCanvas } = require('canvas');
const os = require('os');
const replaceExtension = require('replace-ext');
const del = require('del');

const mfcSize = 13;
const tempDir = os.tmpdir();

function parseMfcs(stringValue) {
	return stringValue
		.split(/\r?\n/)
		.filter(line => Boolean(line.trim()))
		.map(line => line.split('\t').map(parseFloat));
}

function normalizeNumber(value, { min, max }) {
	const result = (value - min) / (max - min);
	return Math.min(1, Math.max(0, result));
}

function normalizeMfcs(mfcs) {
	const limits = [];
	for (let i = 0; i < mfcSize; i++) {
		const column = mfcs.map(mfc => mfc[i]);
		limits.push({ min: Math.min(...column), max: Math.max(...column) });
	}
	return mfcs.map(mfc => mfc.map((value, i) => normalizeNumber(value, limits[i])));
}

function getHeatMapColor(value) {
	return [1 - value, 1 - value, 1 - value];

	// This code creates a heat map from black through blue and red to yellow.
	// Though visually more impressive, I find it harder to read than the
	// simple grayscale map above.
	return [
		normalizeNumber(value, { min: 0.25, max: 0.5 }),
		normalizeNumber(value, { min: 0.75, max: 1.0 }),
		value < 0.25 ? value * 4 : normalizeNumber(-value, { min: -0.75, max: -0.5 }),
	];
}

function colorToStyle([r, g, b]) {
	return `rgb(${Math.round(r * 255)}, ${Math.round(g * 255)}, ${Math.round(b * 255)})`;
}

function createMfcCanvas(mfc) {
	const coefficientWidth = 16;
	const coefficientHeight = 16;
	const canvas = createCanvas(coefficientWidth, mfcSize * coefficientHeight);
	const ctx = canvas.getContext('2d');
	mfc.forEach((mfcc, i) => {
		ctx.fillStyle = colorToStyle(getHeatMapColor(mfcc));
		ctx.fillRect(0, i * coefficientHeight, coefficientWidth, coefficientHeight);
	});
	return canvas;
}

async function main() {
	const args = process.argv.slice(2);
	if (args.length !== 1) {
		throw new Error(`Expected one argument (input audio file) but got ${args.length}.`);
	}
	
	const [inputPath] = args;
	
	console.log('Extracting MFCCs.');
	const maxBuffer = 10 * 1024 * 1024;
	const mfccString = (await exec(`extract-mfcc "${inputPath}"`, { maxBuffer })).stdout;
	const rawMfcs = parseMfcs(mfccString);
	const mfcs = normalizeMfcs(rawMfcs);

	console.log('Creating frame images.');
	await del([`${tempDir}/mfc-*.png`], { force: true });
	for (let frameIndex = 0; frameIndex < mfcs.length; frameIndex++) {
		const canvas = createMfcCanvas(mfcs[frameIndex]);
		await writeFile(`${tempDir}/mfc-${String(frameIndex).padStart(5, '0')}.png`, canvas.toBuffer());
	}

	const outputPath = replaceExtension(inputPath, '-mfcc.mp4');
	console.log(`Assembling output file ${outputPath}.`);
	await exec(`ffmpeg -y -framerate 100 -i ${tempDir}/mfc-%05d.png -i "${inputPath}" -vf format=yuv420p ${outputPath}`);
}

main();
