# matcher decisions

1. `Matcher` is a static-dispatch concept: `try_fill()` returns a `variant<Fill, Reject>`, so a reject carries its reason.
2. `LastTradeMatcher` is the first variation: full instant fill at the last-seen trade price.
3. `CostAwareMatcher` is a second variation.
4. `SimExecution` grew a matcher-taking constructor so matcher variations can carry runtime config.
