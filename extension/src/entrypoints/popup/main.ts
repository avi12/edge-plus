import App from "./App.svelte";
import "./popup.css";
import { mount } from "svelte";

mount(App, { target: document.getElementById("app")! });
