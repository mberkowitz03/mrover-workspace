<!-- This file contains the Perception component which is an abstract component.
     It simulates all the logic that is performed by the rover's perception
     program. -->

<!------------------------------------------- Template -------------------------------------------->
<template>
  <!-- Render nothing. -->
  <div />
</template>


<!-------------------------------------------- Script --------------------------------------------->
<script lang="ts">
import { Component, Vue, Watch } from 'vue-property-decorator';
import { Getter, Mutation } from 'vuex-class';
import {
  ArTag,
  FieldOfViewOptions,
  Gate,
  ObstacleMessage,
  Odom,
  TargetListMessage,
  ZedGimbalPosition,
  ObstacleField
} from '../../utils/types';
import ObstacleDetector from './obstacle_detector';
import TargetDetector from './target_detector';

@Component({})
export default class Perception extends Vue {
  /************************************************************************************************
   * Vuex Getters
   ************************************************************************************************/
  @Getter
  private readonly arTags!:ArTag[];

  @Getter
  private readonly canvasHeight!:number;

  @Getter
  private readonly currOdom!:Odom;

  @Getter
  private readonly falseArTags!:ArTag[];

  @Getter
  private readonly fieldCenterOdom!:Odom;

  @Getter
  private readonly fieldOfViewOptions!:FieldOfViewOptions;

  @Getter
  private readonly fieldSize!:number;

  @Getter
  private readonly gates!:Gate[];

  @Getter
  private readonly obstacles!:ObstacleField[];

  @Getter
  private readonly simulatePercep!:boolean;

  @Getter
  private readonly zedGimbalPos!:ZedGimbalPosition;

  /************************************************************************************************
   * Vuex Mutations
   ************************************************************************************************/
  @Mutation
  private readonly setObstacleMessage!:(newObstacle:ObstacleMessage)=>void;

  @Mutation
  private readonly setTargetList!:(newTargetList:TargetListMessage)=>void;

  /************************************************************************************************
   * Private Members
   ************************************************************************************************/
  private obstacleDetector!:ObstacleDetector;
  private targetDetector!:TargetDetector;


  /************************************************************************************************
   * Watchers
   ************************************************************************************************/
  /* When the list of AR tags changes, update it in the target detector and
     recompute visible targets. */
  @Watch('arTags', { deep: true })
  private onArTagsChange():void {
    /* If targetDetector not yet defined */
    if (this.targetDetector === undefined) {
      return;
    }

    /* Recompute targetList LCM */
    this.targetDetector.updateArTags(this.arTags);
    this.computeVisibleTargets();
  } /* onArTagsChange() */

  /* When the canvas changes, update it in the obstacle detector and the target
     detector and recompute visible obstacles and targets. */
  @Watch('canvasHeight')
  private onCanvasHeightChange():void {
    /* If obstacleDetector not yet defined */
    if (this.obstacleDetector === undefined) {
      return;
    }

    /* Update canvas parameters in obstacleDetector. */
    this.obstacleDetector.updateCanvasHeight(this.canvasHeight);
    const scale = this.canvasHeight / this.fieldSize;
    this.obstacleDetector.updateScale(scale);
  } /* onCanvasHeightChange() */

  /* When our current odom changes, update it in the obstacle detector and the
     target detector and recompute visible obstacles and targets. */
  @Watch('currOdom', { deep: true })
  private onCurrOdomChange():void {
    /* If obstacleDetector or targetDetector not yet defined */
    if (this.obstacleDetector === undefined || this.targetDetector === undefined) {
      return;
    }

    /* Recompute obstacle LCM */
    this.obstacleDetector.updateCurrOdom(this.currOdom);
    this.computeVisibleObstacles();

    /* Recompute targetList LCM */
    this.targetDetector.updateCurrOdom(this.currOdom);
    this.computeVisibleTargets();
  } /* onCurrOdomChange() */

  /* When the field of view changes, update it in the obstacle detector and the
     target detector and recompute visible obstacles and targets. */
  @Watch('fieldOfViewOptions', { deep: true })
  private onFovChange():void {
    /* If obstacleDetector or targetDetector not yet defined */
    if (this.obstacleDetector === undefined || this.targetDetector === undefined) {
      return;
    }

    /* Recompute obstacle LCM */
    this.obstacleDetector.updateFov(this.fieldOfViewOptions);
    this.computeVisibleObstacles();

    /* Recompute targetList LCM */
    this.targetDetector.updateFov(this.fieldOfViewOptions);
    this.computeVisibleTargets();
  } /* onFovChange() */

  /* When the field changes, update it in the obstacle detector and the target
     detector and recompute visible obstacles and targets. */
  @Watch('fieldSize')
  private onFieldSizeChange():void {
    /* If obstacleDetector not yet defined */
    if (this.obstacleDetector === undefined) {
      return;
    }

    /* Update canvas parameters in obstacleDetector. */
    const scale = this.canvasHeight / this.fieldSize;
    this.obstacleDetector.updateScale(scale);
  } /* onFieldSizeChange() */

  /* When the list of gates changes, update it in the target detector and
     recompute visible targets. */
  @Watch('gates', { deep: true })
  private onGatesChange():void {
    /* If targetDetector not yet defined */
    if (this.targetDetector === undefined) {
      return;
    }

    /* Recompute targetList LCM */
    this.targetDetector.updateGates(this.gates);
    this.computeVisibleTargets();
  } /* onGatesChange() */

  /* When the list of obstacles changes, update it in the obstacle detector and
     recompute visible targets. */
  @Watch('obstacles', { deep: true })
  private onObstaclesChange():void {
    /* If obstacleDetector not yet defined */
    if (this.obstacleDetector === undefined) {
      return;
    }

    /* Recompute obstacle LCM */
    this.obstacleDetector.updateObstacles(this.obstacles);
    this.computeVisibleObstacles();
  } /* onObstaclesChange() */

  /* When starting simulating perception, compute visible obstacles and
     targets. We intentionally only update when starting simulating because
     when we stop simulating perception, the last LCM messages are what nav
     will still see (i.e. we don't send a "blank" message. */
  @Watch('simulatePercep')
  private onSimulatePercepChange():void {
    if (this.simulatePercep) {
      this.computeVisibleObstacles();
      this.computeVisibleTargets();
    }
  } /* onSimulatePercepChange() */

  @Watch('zedGimbalPos', { deep: true })
  private onZedGimbalPosChange():void {
    /* If obstacleDetector or targetDetector not yet defined */
    if (this.obstacleDetector === undefined || this.targetDetector === undefined) {
      return;
    }

    /* Recompute obstacle LCM */
    this.obstacleDetector.updateZedGimbalPos(this.zedGimbalPos);
    this.computeVisibleObstacles();

    /* Recompute targetList LCM */
    this.targetDetector.updateZedGimbalPos(this.zedGimbalPos);
    this.computeVisibleTargets();
  }

  /************************************************************************************************
   * Private Methods
   ************************************************************************************************/
  /* Compute the obstacles that are visible to the rover. */
  private computeVisibleObstacles():void {
    /* If not simulating perception */
    if (!this.simulatePercep) {
      return;
    }

    /* If obstacleDetector not yet defined */
    if (this.obstacleDetector === undefined) {
      return;
    }

    /* Recompute obstacle LCM */
    const obsMsg:ObstacleMessage = this.obstacleDetector.computeObsMsg();
    this.setObstacleMessage(obsMsg);
  } /* computeVisibleObstacles() */

  /* Compute the targets that are visible to the rover. */
  private computeVisibleTargets():void {
    /* If not simulating perception */
    if (!this.simulatePercep) {
      return;
    }

    /* If targetDetector not yet defined */
    if (this.targetDetector === undefined) {
      return;
    }

    /* Recompute targetList LCM */
    const targetList:TargetListMessage = this.targetDetector.computeTargetList(this.falseArTags);
    this.setTargetList(targetList);
  } /* computeVisibleTargets() */

  /************************************************************************************************
   * Vue Life Cycle
   ************************************************************************************************/
  /* Create obstacle detector and target detector objects. */
  private created():void {
    const scale = this.canvasHeight / this.fieldSize;

    this.obstacleDetector = new ObstacleDetector(this.currOdom, this.zedGimbalPos, this.obstacles,
                                                 this.fieldOfViewOptions, this.fieldCenterOdom,
                                                 this.canvasHeight, scale);
    this.computeVisibleObstacles();

    this.targetDetector = new TargetDetector(this.currOdom, this.zedGimbalPos, this.arTags,
                                             this.gates, this.fieldOfViewOptions,
                                             this.fieldCenterOdom);
    this.computeVisibleTargets();
  } /* created() */
}
</script>
