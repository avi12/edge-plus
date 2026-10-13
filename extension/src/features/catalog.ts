import catalog from "./catalog.json";

/** Also the keys of the app's settings file, so they are the persisted contract */
export enum FeatureId {
  splitPanes = "splitPanes",
  paneFocus = "paneFocus"
}

export const HOST_NAME = catalog.hostName;

export const FEATURES = catalog.features.map(feature => ({ ...feature, id: feature.id as FeatureId }));
