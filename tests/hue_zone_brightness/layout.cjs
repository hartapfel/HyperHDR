const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const controls = new Map();
const writes = [];
const context = {
    window: {serverConfig: {leds: [], device: {}, smoothing: {}, color: {channelAdjustment: [{}]}}},
    conf_editor: {getEditor: key => ({getValue: () => ({
        zoneBrightnessBalance: 65, shadowLift: 35, convertToLinearRgb: true, screenColorimetry: true
    })[key.split('.').pop()]})},
    requestWriteConfig: config => writes.push(structuredClone(config)),
    $: id => ({val(value) {
        if (value !== undefined) controls.set(id, value);
        return controls.get(id);
    }}),
};
vm.createContext(context);
const source = fs.readFileSync(path.join(__dirname, '../../www/js/LedSetups/SetupPhilipsHue.js'), 'utf8');
vm.runInContext(source, context);
const zones = positions => JSON.parse(JSON.stringify(context.hueScreenZones(positions)));
const layout = zones({9: [-1, 0, 0], 4: [0, 0, 0], 12: [1, 0, 0], 13: [1, -1, 0], 14: null});
assert.deepEqual(Object.keys(layout), ['4', '9', '12', '13']);
assert.deepEqual([layout[9].hmax, layout[4].hmin, layout[4].hmax, layout[12].hmin], [.25, .25, .75, .75]);
assert.deepEqual(layout[12], layout[13]);
const vertical = zones({0: [0, 1, .85], 7: [0, 0, -.35]});
assert.ok(Math.abs(vertical[0].vmax - .375) < 1e-9);
assert.equal(vertical[0].vmax, vertical[7].vmin);

context.useV2Api = true;
context.hueType = 'philipshueentertainment';
context.groupIDs = {old: {channels: [{channel_id: 8, position: {x: 0, y: 0, z: 0}}]},
    current: {channels: [{channel_id: 9, position: {x: 1, y: 0, z: 0}},
                         {channel_id: 2, position: {x: -1, y: 0, z: 0}}]}};
context.get_hue_lights = () => { context.lightIDs = context.channels; };
context.useGroupId('old');
context.useGroupId('current');
assert.equal(writes.length, 0);
assert.deepEqual(Array.from(context.groupLights), ['9', '2']);
controls.set('#hue_2', 'bridge');
controls.set('#hue_9', 'bridge');
context.SaveHueConfig();
const {leds, device} = writes[0];
assert.deepEqual(device.lightIds, ['2', '9']);
assert.equal(leds[0].hmax, .5);
assert.equal(leds[1].hmin, .5);
assert.equal(device.entertainmentConfigurationId, 'current');
assert.equal(device.zoneBrightnessBalance, 65);
assert.equal(device.shadowLift, 35);
assert.equal(device.convertToLinearRgb, true);
assert.equal(device.screenColorimetry, true);
console.log('Hue layout import and wizard save passed');
