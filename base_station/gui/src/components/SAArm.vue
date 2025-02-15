<template>
  <div class="wrap">
    <h3> Arm Controls </h3>
    <div class="controls">
      <Checkbox ref="open-loop" v-bind:name="'Open Loop'" v-on:toggle="updateControlMode('open-loop', $event)"/>
      <Checkbox ref="closed-loop" v-bind:name="'Closed Loop'" v-on:toggle="updateControlMode('closed-loop', $event)"/>
    </div>
    <div class="joint-b-calibration" v-if='!this.jointBIsCalibrated && this.controlMode !== "calibrating"'>
      Joint B not calibrated!
      <button v-on:click="startCalibration()">Calibrate</button>
    </div>
    <div class="joint-b-calibration" v-if='!this.jointBIsCalibrated && this.controlMode === "calibrating"'>
      Calibrating Joint B...
      <button v-on:click="abortCalibration()">Abort</button>
    </div>
    <div class="joints">
        <div>joint a: {{SAPosition.joint_a}}</div>
        <div>joint b: {{SAPosition.joint_b}}</div>
        <div>joint c: {{SAPosition.joint_c}}</div>
        <div>joint e: {{SAPosition.joint_e}}</div>
    </div>
    <h4> Preset values </h4>
    <div class="buttons">
      <button v-on:click="send_position('stowed')">Stowed</button>
      <button v-on:click="send_position('scoop')">Scoop</button>
      <button v-on:click="send_position('deposit')">Deposit</button>
      <button v-on:click="send_position('raman')">Raman</button>
      <button v-on:click="send_path('intermediate')">intermediate</button>
    </div>
    <div class="buttons">
      <Checkbox ref="sim-mode" v-bind:name="'Sim Mode'" v-on:toggle="updateSimMode($event)"/>
      <button v-on:click="zeroPositionCallback()">Set Zero Position</button>
    </div>
  </div>
</template>

<script>
import Checkbox from './Checkbox.vue'

let interval;

export default {
  data() {
    return {
      controlMode: 'off',
      xboxControlEpsilon: 0.4,
      SAPosition: {
        joint_a: 0,
        joint_b: 0,
        joint_c: 0,
        joint_e: 0
      },
      position: "stowed",
      enable_limit_switch: true,
      
      jointBIsCalibrated: false,
      calibrationTimer: -1
    }
  },

  beforeDestroy: function () {
    window.clearInterval(interval);
  },


  created: function () {
    this.$parent.subscribe('/sa_offset_pos', (msg) => {
      this.SAPosition = msg
    })

    // Subscribe to requests to change state from IK backend
    this.$parent.subscribe('/arm_control_state', (msg) => {
      console.log('received new state: ' + msg.state)
      var new_state = msg.state
      // If new state matches current state
      if (new_state === this.controlMode) {
        return
      }
      // If new state is not current state, turn off current state
      if (this.controlMode === 'closed-loop' || this.controlMode === 'open-loop') {
        this.$refs[this.controlMode].toggle()
      }
      if (new_state === 'closed-loop' || new_state === 'open-loop') {
        this.$refs[new_state].toggle()
      }
      
      this.controlMode = new_state
    })

    this.$parent.subscribe('/ra_b_calib_data', (msg) => {
      this.updateCalibrationStatus(msg)
    })

    this.$parent.subscribe('/sa_b_calib_data', (msg) => {
      this.updateCalibrationStatus(msg)
    })
  
    const XBOX_CONFIG = {
      'left_js_x': 0,
      'left_js_y': 1,
      'left_trigger': 6,
      'right_trigger': 7,
      'right_js_x': 2,
      'right_js_y': 3,
      'right_bumper': 5,
      'left_bumper': 4,
      'd_pad_up': 12,
      'd_pad_down': 13,
      'd_pad_right': 14,
      'd_pad_left': 15,
      'a': 0,
      'b': 1,
      'x': 2,
      'y': 3
    }

    const updateRate = 0.1
    interval = window.setInterval(() => {
      const gamepads = navigator.getGamepads()
      for (let i = 0; i < 4; i++) {
        const gamepad = gamepads[i]
        if (gamepad) {
          if (gamepad.id.includes('Microsoft') || gamepad.id.includes('Xbox')) {
            const xboxData = {
              'type': 'Xbox',
              'left_js_x': gamepad.axes[XBOX_CONFIG['left_js_x']], // shoulder rotate
              'left_js_y': gamepad.axes[XBOX_CONFIG['left_js_y']], // shoulder tilt
              'left_trigger': gamepad.buttons[XBOX_CONFIG['left_trigger']]['value'], // elbow forward
              'right_trigger': gamepad.buttons[XBOX_CONFIG['right_trigger']]['value'], // elbow back
              'right_js_x': gamepad.axes[XBOX_CONFIG['right_js_x']], // hand rotate
              'right_js_y': gamepad.axes[XBOX_CONFIG['right_js_y']], // hand tilt
              'right_bumper': gamepad.buttons[XBOX_CONFIG['right_bumper']]['pressed'], // grip close
              'left_bumper': gamepad.buttons[XBOX_CONFIG['left_bumper']]['pressed'], // grip open
              'd_pad_up': gamepad.buttons[XBOX_CONFIG['d_pad_up']]['pressed'],
              'd_pad_down': gamepad.buttons[XBOX_CONFIG['d_pad_down']]['pressed'],
              'd_pad_right': gamepad.buttons[XBOX_CONFIG['d_pad_right']]['pressed'],
              'd_pad_left': gamepad.buttons[XBOX_CONFIG['d_pad_left']]['pressed'],
              'a': gamepad.buttons[XBOX_CONFIG['a']]['pressed'],
              'b': gamepad.buttons[XBOX_CONFIG['b']]['pressed'],
              'x': gamepad.buttons[XBOX_CONFIG['x']]['pressed'],
              'y': gamepad.buttons[XBOX_CONFIG['y']]['pressed']
            }
            if (this.controlMode !== 'open-loop' && this.checkXboxInput(xboxData)) {
              this.forceOpenLoop()
            }
            if (this.controlMode === 'open-loop') {
              this.$parent.publish('/sa_control', xboxData)
            }
          }
        }
      }
    }, updateRate*1000)
  },

  methods: {
    send_position: function(preset) {
      console.log(preset);
      this.$parent.publish("/arm_preset", {
        'type': 'ArmPreset',
        'preset': preset
      })
    },

    send_path: function(preset) {
      this.$parent.publish("/arm_preset_path", {
        'type': 'ArmPresetPath',
        'preset': preset
      })
    },

    updateCalibrationStatus: function(msg) {
      if (msg.calibrated && !this.jointBIsCalibrated) {
        clearTimeout(this.calibrationTimer)

        const armStateMsg = {
          'type': 'ArmControlState',
          'state': 'off'
        }

        this.$parent.publish('/arm_control_state', armStateMsg)
      }

      this.jointBIsCalibrated = msg.calibrated
    },

    updateControlMode: function (mode, checked) {

      if (checked) {
        // control mode can either be open loop or control mode, not both
        if (this.controlMode !== '' && this.controlMode !== 'off'){
          this.$refs[this.controlMode].toggle()
        }

        this.controlMode = mode;
      }
      else {
        this.controlMode = 'off';
      }

      const armStateMsg = {
        'type': 'ArmControlState',
        'state': this.controlMode
      }

      this.$parent.publish('/arm_control_state', armStateMsg);
    },

    forceOpenLoop: function () {
      console.log(this.controlMode)

      if (this.controlMode === 'open-loop') {
        return
      }

      if (this.controlMode === 'closed-loop') {
        this.$refs['closed-loop'].toggle()
      }

      this.$refs['open-loop'].toggle()
      this.controlMode = 'open-loop'

      const armStateMsg = {
        'type': 'ArmControlState',
        'state': this.controlMode
      }

      this.$parent.publish('/arm_control_state', armStateMsg)
    },

    checkXboxInput: function (xboxData) {
      if (Math.abs(xboxData['left_js_x']) > this.xboxControlEpsilon) {
        return true
      }
      if (Math.abs(xboxData['left_js_y']) > this.xboxControlEpsilon) {
        return true
      }
      if (Math.abs(xboxData['left_trigger']) > this.xboxControlEpsilon) {
        return true
      }
      if (Math.abs(xboxData['right_trigger']) > this.xboxControlEpsilon) {
        return true
      }
      if (Math.abs(xboxData['right_js_x']) > this.xboxControlEpsilon) {
        return true
      }
      if (Math.abs(xboxData['right_js_y']) > this.xboxControlEpsilon) {
        return true
      }
      if (xboxData['left_bumper']) {
        return true
      }
      if (xboxData['right_bumper']) {
        return true
      }
      if (xboxData['a']) {
        return true
      }
      if (xboxData['b']) {
        return true
      }
      if (xboxData['x']) {
        return true
      }
      if (xboxData['y']) {
        return true
      }
      if (xboxData['d_pad_up']) {
        console.log('d')
        return true
      }
      if (xboxData['d_pad_down']) {
        return true
      }
      if (xboxData['d_pad_left']) {
        return true
      }
      if (xboxData['d_pad_right']) {
        return true
      }
      return false
    },

    toggle_limit_switch_status: function() {
      console.log("Setting limit switch enabled status: " + this.enable_limit_switch);
      this.$parent.publish("/scoop_limit_switch_enable_cmd", {
        'type': 'Enable',
        'enabled': this.enable_limit_switch
      });
    },

    zeroPositionCallback: function() {
	    console.log("publishing zero position")
      this.$parent.publish('/zero_position', { 'type': 'Signal' });
    },

    updateSimMode: function(checked) {
      this.sim_mode = checked

      const simModeMsg = {
        'type': 'SimulationMode',
        'sim_mode': checked
      }
      this.$parent.publish('/simulation_mode', simModeMsg);
    },

    startCalibration: function() {
      const armStateMsg = {
        'type': 'ArmControlState',
        'state': 'calibrating'
      }

      this.$parent.publish('/arm_control_state', armStateMsg)

      this.calibrationTimer = setTimeout(() => {
        this.abortCalibration()
      }, 20000)
    },

    abortCalibration: function() {
      clearTimeout(this.calibrationTimer)
      
      const armStateMsg = {
        'type': 'ArmControlState',
        'state': 'off'
      }

      this.$parent.publish('/arm_control_state', armStateMsg)
    },
  },

  components: {
    Checkbox
  }
}
</script>

<style scoped>

.wrap {
  display: inline-block;
  align-items: center;
  justify-items: center;
  width: 100%;
}
.controls {
  display: flex;
  align-items:center;
}
.header {
  display:flex;
  align-items:center;
}

.joints {
  display: grid;
  grid-template-rows: 1fr 1fr;
  grid-auto-flow: column;
  padding-bottom: 2vh;
}

.buttons {
  display: grid;
  grid-template-columns: 1fr 1fr 1fr;
}

.joint-b-calibration {
  display: flex;
  gap: 10px;
  width: 250px;
  font-weight: bold;
  color: red;
  padding-bottom: 2vh;
}

</style>