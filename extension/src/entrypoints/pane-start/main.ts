import App from "./App.svelte";
import "./pane-start.css";
import { mount } from "svelte";

mount(App, { target: document.getElementById("app")! });
