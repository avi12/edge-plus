import App from "./App.svelte";
import "./split.css";
import { mount } from "svelte";

mount(App, { target: document.getElementById("app")! });
