import { z } from "zod";

// What the mount test's window thread reports (mount-all.svelte.ts) and the
// test reads back (mount-all.test.ts): one schema, so neither side restates it.

const failureSchema = z.object({
  name: z.string(),
  step: z.string(),
  message: z.string()
});

const countsSchema = z.object({
  properties: z.number(),
  events: z.number(),
  attached: z.number(),
  bindables: z.number()
});

export const reportSchema = z.object({
  components: z.number(),
  mounted: z.number(),
  ...countsSchema.shape,
  exercised: countsSchema,
  behaviours: z.number(),
  failures: z.array(failureSchema)
});

export type Failure = z.infer<typeof failureSchema>;
export type Counts = z.infer<typeof countsSchema>;
export type Report = z.infer<typeof reportSchema>;
