#include "PPO.h"
#include "model.h"

// Vector of tensors.
using VT = std::vector<torch::Tensor>;

// Optimizer.
using OPT = torch::optim::Optimizer;

class RLagent{
    public:
        ActorCritic ac;
        torch::optim::Adam opt;

        // uint n_iter = 10000;
        uint n_steps = 2048;
        // uint n_epochs = 15;
        uint mini_batch_size = 512;
        uint ppo_epochs = 4;
        double beta = 1e-3;

        VT states;
        VT actions;
        VT rewards;
        VT dones;

        VT log_probs;
        VT returns;
        VT values;

        // Counter.
        uint c = 0;

        // Average reward.
        // double best_avg_reward = 0.;
        // double avg_reward = 0.;

        RLagent(uint n_in=9, uint n_out=8, double std=2e-2) : ac(n_in, n_out, std), opt(ac->parameters(), 1e-3)
        {
            ac->to(torch::kF64);
            ac->normal(0., std);
        }

        torch::Tensor RLAgent_act(torch::Tensor state)
        {
            states.push_back(state);
            auto av = ac->forward(state);
            actions.push_back(std::get<0>(av));
            values.push_back(std::get<1>(av));
            log_probs.push_back(ac->log_prob(actions[c]));

            return actions[c];
        }

        void RLAgent_update(torch::Tensor reward)
        {
            rewards.push_back(reward);
            // avg_reward += reward / n_iter;
            c++;

            if (c % n_steps == 0){
                printf("Updating the network.\n");
                values.push_back(std::get<1>(ac->forward(states[c - 1])));

                returns = PPO::returns(rewards, dones, values, .99, .95);

                torch::Tensor t_log_probs = torch::cat(log_probs).detach();
                torch::Tensor t_returns = torch::cat(returns).detach();
                torch::Tensor t_values = torch::cat(values).detach();
                torch::Tensor t_states = torch::cat(states);
                torch::Tensor t_actions = torch::cat(actions);
                torch::Tensor t_advantages = t_returns - t_values.slice(0, 0, n_steps);

                PPO::update(ac, t_states, t_actions, t_log_probs, t_returns, t_advantages, opt, n_steps, ppo_epochs, mini_batch_size, beta);

                c = 0;

                states.clear();
                actions.clear();
                rewards.clear();
                dones.clear();

                log_probs.clear();
                returns.clear();
                values.clear();
            }
        }
};